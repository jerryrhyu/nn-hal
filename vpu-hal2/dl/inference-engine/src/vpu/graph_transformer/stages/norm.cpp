//
// INTEL CONFIDENTIAL
// Copyright 2017-2018 Intel Corporation.
//
// The source code contained or described herein and all documents
// related to the source code ("Material") are owned by Intel Corporation
// or its suppliers or licensors. Title to the Material remains with
// Intel Corporation or its suppliers and licensors. The Material may
// contain trade secrets and proprietary and confidential information
// of Intel Corporation and its suppliers and licensors, and is protected
// by worldwide copyright and trade secret laws and treaty provisions.
// No part of the Material may be used, copied, reproduced, modified,
// published, uploaded, posted, transmitted, distributed, or disclosed
// in any way without Intel's prior express written permission.
//
// No license under any patent, copyright, trade secret or other
// intellectual property right is granted to or conferred upon you by
// disclosure or delivery of the Materials, either expressly, by implication,
// inducement, estoppel or otherwise. Any license under such intellectual
// property rights must be express and approved by Intel in writing.
//
// Include any supplier copyright notices as supplier requires Intel to use.
//
// Include supplier trademarks or logos as supplier requires Intel to use,
// preceded by an asterisk. An asterisked footnote can be added as follows:
// *Third Party trademarks are the property of their respective owners.
//
// Unless otherwise agreed by Intel in writing, you may not remove or alter
// this notice or any other notice embedded in Materials by Intel or Intel's
// suppliers or licensors in any way.
//

#include "graph_transformer_impl.hpp"
#include <vector>

void VpuLRNStage::dumpToDot(std::ostream& os) {
    os << "size=" << size << "\\n"
       << "k=" << k << "\\n"
       << "alpha=" << alpha << "\\n"
       << "beta=" << beta;
}

void VpuLRNStage::dumpToBlob(BlobWriter& writer) {
    writer.write(static_cast<uint32_t>(size));
    writer.write(PrecisionUtils::f32tof16(k));
    writer.write(PrecisionUtils::f32tof16(alpha));
    writer.write(PrecisionUtils::f32tof16(beta));
    writer.write(PrecisionUtils::f32tof16(0));  // for alignment

    inputs[0]->dumpToBlob(writer);
    outputs[0]->dumpToBlob(writer);
}

void GraphTransformerImpl::parseNorm(const CNNLayerPtr& _layer,
                                     const std::vector<VpuDataHandle>& inputs,
                                     const std::vector<VpuDataHandle>& outputs) {
    assert(inputs.size() == 1);
    assert(outputs.size() == 1);

    auto layer = std::dynamic_pointer_cast<NormLayer>(_layer);
    assert(layer != nullptr);

    addNewStage<VpuLRNStage>(
        layer->name,
        kLRN,
        layer,
        [layer](VpuLRNStage* stage) {
            stage->size = layer->_size;
            stage->k = layer->_k;
            stage->alpha = layer->_alpha;
            stage->beta = layer->_beta;
        },
        inputs,
        outputs);
}
