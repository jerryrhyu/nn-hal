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
#include <memory>

void VpuFullyConnectedStage::dumpToBlob(BlobWriter& writer) {
    inputs[0]->dumpToBlob(writer);
    outputs[0]->dumpToBlob(writer);
    inputs[1]->dumpToBlob(writer);

    VpuData fakeBiases;
    fakeBiases.dumpToBlob(writer);
}

void GraphTransformerImpl::checkBatchFC(const CNNLayerPtr& layer,
                                               const std::vector<VpuDataHandle>& inputs,
                                               const std::vector<VpuDataHandle>& outputs) {
    assert(layer != nullptr);

    auto input = inputs[0];
    auto output = outputs[0];

    if ((output->dims[Dim::X] != 1) || (output->dims[Dim::Y] != 1)) {
         THROW_IE_EXCEPTION << "[VPU] FC output " << layer->name << " has invalid batch";
    }
}

void GraphTransformerImpl::parseFullyConnected(const CNNLayerPtr& _layer,
                                               const std::vector<VpuDataHandle>& inputs,
                                               const std::vector<VpuDataHandle>& outputs) {
    assert(inputs.size() == 1);
    assert(outputs.size() == 1);

    auto layer = std::dynamic_pointer_cast<FullyConnectedLayer>(_layer);
    assert(layer != nullptr);

    auto input = inputs[0];
    auto output = outputs[0];

    VpuDims wDims(3);
    wDims[Dim::X] = input->dims[Dim::X] * input->dims[Dim::Y];
    wDims[Dim::Y] = input->dims[Dim::Z];
    wDims[Dim::Z] = output->dims.totalSize();

    assert(layer->_weights != nullptr);
    auto weights = addNewData(
        newDataId(),
        [layer, wDims](VpuData* data) {
            data->name = layer->name + "@weights";
            data->index = IndexBlob;
            data->type = VpuDataType::FP16;
            data->order = orderXYZ;
            data->dims = wDims;
            data->strides = calcStrides(data->dims, data->type, data->order);
            data->writer = std::make_shared<DefaultWeightsWriter>(data->dims, layer->_weights);
        });

    auto fcStage = addNewStage<VpuFullyConnectedStage>(
        layer->name,
        kFC,
        layer,
        [](VpuFullyConnectedStage* /*stage*/) {
        },
        {input, weights},
        {output});

    if (layer->_biases != nullptr) {
        auto biases = addNewData(
            newDataId(),
            [layer, this](VpuData* data) {
                data->name = layer->name + "@biases";
                data->index = IndexBlob;
                data->type = VpuDataType::FP16;
                data->dims = VpuDims({static_cast<uint32_t>(layer->_biases->size()), 1, 1});
                data->strides = calcStrides(data->dims, data->type, data->order);
                data->writer = std::make_shared<DefaultBiasesWriter>(layer->_biases);
            });

        addNewStage<VpuBiasStage>(
            biases->name,
            kBias,
            layer,
            [](VpuBiasStage* /*stage*/) {
            },
            {output, biases},
            {output},
            fcStage);
    }
}
