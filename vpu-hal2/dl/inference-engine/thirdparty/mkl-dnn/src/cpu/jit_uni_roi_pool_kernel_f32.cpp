/*******************************************************************************
* Copyright 2017 Intel Corporation
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/
#include "c_types_map.hpp"
#include "nstl.hpp"
#include "utils.hpp"
#include "cpu_roi_pooling_pd.hpp"

#include "jit_uni_roi_pool_kernel_f32.hpp"

namespace mkldnn {
namespace impl {
namespace cpu {

using namespace Xbyak;
using namespace alg_kind;

#define GET_OFF(field) offsetof(jit_roi_pool_call_s, field)

template <cpu_isa_t isa>
status_t jit_uni_roi_pool_kernel_f32<isa>::init_conf(jit_roi_pool_conf_t &jpp,
            const roi_pooling_desc_t &pd, const memory_desc_wrapper &src_d,
            const memory_desc_wrapper &dst_d) {

    const int simd_w = isa == avx512_common ? 16 : 8;
    jpp.c_block = simd_w;

    jpp.mb = dst_d.dims()[0];
    jpp.c  = src_d.dims()[1];
    jpp.ih = src_d.dims()[2];
    jpp.iw = src_d.dims()[3];
    jpp.oh = dst_d.dims()[2];
    jpp.ow = dst_d.dims()[3];

    jpp.spatial_scale = pd.spatial_scale;
    jpp.pooled_h = pd.pooled_h;
    jpp.pooled_w = pd.pooled_w;

    jpp.nb_c = jpp.c / jpp.c_block;

    jpp.nb_c_blocking = isa == avx512_common ? 15 : 7;

    return status::success;
}

template <cpu_isa_t isa>
void jit_uni_roi_pool_kernel_f32<isa>::empty_roi(int c_blocks) {
    uni_vpxor(vmm_zero, vmm_zero, vmm_zero);
    for (int i = 0; i < c_blocks; i++) {
        uni_vmovups(ptr[reg_output + i * jpp.oh * jpp.ow * jpp.c_block * sizeof(float)], vmm_zero);
    }
}

template <cpu_isa_t isa>
void jit_uni_roi_pool_kernel_f32<isa>::roi_pool(int c_blocks, const char* h_loop_label, const char* w_loop_label) {
    mov(aux_reg_input, reg_input);

    for (int i = 0; i < c_blocks; i++) {
        Vmm vmm_max = get_acc_reg(i);
        uni_vmovups(vmm_max, ptr[reg_input + i * jpp.ih * jpp.iw * jpp.c_block * sizeof(float)]);
    }

    xor_(h_iter, h_iter);
    L(h_loop_label); {
        xor_(w_iter, w_iter);
        mov(aux_reg_input1, aux_reg_input);
        L(w_loop_label); {
            for (int i = 0; i < c_blocks; i++) {
                Vmm vmm_max = get_acc_reg(i);
                Vmm vmm_src = get_src_reg(i);

                uni_vmovups(vmm_src, ptr[aux_reg_input1 + i * jpp.ih * jpp.iw * jpp.c_block * sizeof(float)]);
                if (isa == sse42) {
                    movups(vmm_mask, vmm_max);
                    cmpps(vmm_mask, vmm_src, _cmp_lt_os);
                    blendvps(vmm_max, vmm_src);
                } else if (isa == avx2) {
                    vcmpps(vmm_mask, vmm_max, vmm_src, _cmp_lt_os);
                    vblendvps(vmm_max, vmm_max, vmm_src, vmm_mask);
                } else if (isa == avx512_common) {
                    vcmpps(k_store_mask,  vmm_max,  vmm_src, _cmp_lt_os);
                    vblendmps(vmm_max| k_store_mask, vmm_max, vmm_src);
                }
            }

            add(aux_reg_input1, jpp.c_block * sizeof(float));

            inc(w_iter);
            cmp(w_iter, reg_kw);
            jl(w_loop_label, T_NEAR);
        }

        add(aux_reg_input, jpp.iw * jpp.c_block * sizeof(float));

        inc(h_iter);
        cmp(h_iter, reg_kh);
        jl(h_loop_label, T_NEAR);
    }

    for (int i = 0; i < c_blocks; i++) {
        Vmm vmm_dst = get_acc_reg(i);
        uni_vmovups(ptr[reg_output + i * jpp.oh * jpp.ow * jpp.c_block * sizeof(float)], vmm_dst);
    }
}

template <cpu_isa_t isa>
void jit_uni_roi_pool_kernel_f32<isa>::generate() {
    this->preamble();

    const char *tail_label = "tail";
    const char *exit_label = "exit";
    const char *empty_roi_label = "empty_roi";
    const char *empty_roi_tail_label = "empty_roi_tail";

    mov(reg_input, ptr[this->param1 + GET_OFF(src)]);
    mov(reg_output, ptr[this->param1 + GET_OFF(dst)]);

    mov(reg_kh, ptr[this->param1 + GET_OFF(kh)]);
    mov(reg_kw, ptr[this->param1 + GET_OFF(kw)]);
    mov(reg_bin_area, ptr[this->param1 + GET_OFF(bin_area)]);
    mov(reg_c_blocks, ptr[this->param1 + GET_OFF(c_blocks)]);

    int nb_c_tail = jpp.nb_c % jpp.nb_c_blocking;

    cmp(reg_c_blocks, jpp.nb_c_blocking);
    jne(nb_c_tail ? tail_label : exit_label, T_NEAR);

    cmp(reg_bin_area, 0);
    je(empty_roi_label, T_NEAR);

    roi_pool(jpp.nb_c_blocking, "h_loop", "w_loop");
    if (isa == sse42) {
        add(reg_input, 4 * sizeof(float));
        add(reg_output, 4 * sizeof(float));
        roi_pool(jpp.nb_c_blocking, "h_loop_half", "w_loop_half");
    }

    jmp(exit_label, T_NEAR);

    L(empty_roi_label);
    empty_roi(jpp.nb_c_blocking);
    if (isa == sse42) {
        add(reg_output, 4 * sizeof(float));
        empty_roi(jpp.nb_c_blocking);
    }
    jmp(exit_label, T_NEAR);

    if (nb_c_tail) {
        L(tail_label);
        cmp(reg_bin_area, 0);
        je(empty_roi_tail_label, T_NEAR);

        cmp(reg_c_blocks, nb_c_tail);
        jne(exit_label, T_NEAR);
        roi_pool(nb_c_tail, "h_loop_tail", "w_loop_tail");
        if (isa == sse42) {
            add(reg_input, 4 * sizeof(float));
            add(reg_output, 4 * sizeof(float));
            roi_pool(nb_c_tail, "h_loop_tail_half", "w_loop_tail_half");
        }

        jmp(exit_label, T_NEAR);

        L(empty_roi_tail_label);
        empty_roi(nb_c_tail);
        if (isa == sse42) {
            add(reg_output, 4 * sizeof(float));
            empty_roi(nb_c_tail);
        }
        jmp(exit_label, T_NEAR);
    }

    L(exit_label);

    this->postamble();
}

template struct jit_uni_roi_pool_kernel_f32<sse42>;
template struct jit_uni_roi_pool_kernel_f32<avx2>;
template struct jit_uni_roi_pool_kernel_f32<avx512_common>;

}
}
}
