// Copyright (c) 2018 Intel Corporation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

/**
 * @brief This header file provides structures to store info about pre-processing of network inputs (scale, mean image, ...)
 * @file ie_preprocess.hpp
 */
#pragma once

#include "ie_blob.h"
#include <vector>
#include <memory>

namespace InferenceEngine {
/**
 * @struct PreProcessChannel
 * @brief This structure stores info about pre-processing of network inputs (scale, mean image, ...)
 */
struct PreProcessChannel {
    /**
     * Scale parameter for a channel
     */
    float stdScale = 1;
    /**
     * @brief Mean value for a channel
     */
    float meanValue = 0;
    /**
     * @brief Mean data for a channel
     */
    Blob::Ptr meanData;
    /**
     * @brief Smart pointer to an instance
     */
    typedef std::shared_ptr<PreProcessChannel> Ptr;
};

/**
 * @brief Defines available types of mean
 */
typedef enum {
    MEAN_IMAGE,
    MEAN_VALUE,
    NONE,
} MeanVariant;

/**
 * @class PreProcessInfo
 * @brief This class stores pre-process information for the channel
 */
class PreProcessInfo {
    std::vector<PreProcessChannel::Ptr> _channelsInfo;
    MeanVariant _variant = NONE;

public:
    /**
     * @brief Overloaded [] operator to safely get the channel by an index. 
     * Throws an exception if channels are empty.
     * @param index Index of the channel to get
     * @return The pre-process channel instance
     */
    PreProcessChannel::Ptr &operator[](size_t index) {
        if (_channelsInfo.empty()) {
            THROW_IE_EXCEPTION << "accessing pre-process when nothing was set.";
        }
        if (index >= _channelsInfo.size()) {
            THROW_IE_EXCEPTION << "pre process index " << index << " is out of bounds.";
        }
        return _channelsInfo[index];
    }

    /**
     * @brief Overloaded [] operator to safely get the channel by an index. 
     * Throws exception if channels are empty.
     * @param index Index of the channel to get
     * @return The const preprocess channel instance
     */
    const PreProcessChannel::Ptr &operator[](size_t index) const {
        if (_channelsInfo.empty()) {
            THROW_IE_EXCEPTION << "accessing pre-process when nothing was set.";
        }
        if (index >= _channelsInfo.size()) {
            THROW_IE_EXCEPTION << "pre process index " << index << " is out of bounds.";
        }
        return _channelsInfo[index];
    }

    /**
     * @brief Gets a number of channels
     * @return The number of channels
     */
    size_t getNumberOfChannels() const {
        return _channelsInfo.size();
    }

    /**
     * @brief Initializes the given number of channels
     * @param numberOfChannels Number of channels to initialize
     */
    void init(const size_t numberOfChannels) {
        _channelsInfo.resize(numberOfChannels);
        for (auto &channelInfo : _channelsInfo) {
            channelInfo = std::make_shared<PreProcessChannel>();
        }
    }

    /**
     * @brief Checks if mean operation is applicable. 
     * If applicable, sets the mean type to MEAN_IMAGE for all channels
     * @param meanImage Blob with a mean image
     */
    void setMeanImage(const Blob::Ptr &meanImage) {
        if (meanImage.get() == nullptr) {
            THROW_IE_EXCEPTION << "Failed to set invalid mean image: nullptr";
        } else if (meanImage.get()->dims().size() != 3) {
            THROW_IE_EXCEPTION << "Failed to set invalid mean image: number of dimensions != 3";
        } else if (meanImage.get()->dims()[2] != getNumberOfChannels()) {
            THROW_IE_EXCEPTION << "Failed to set invalid mean image: number of channels != "
                               << getNumberOfChannels();
        } else if (meanImage.get()->layout() != Layout::CHW) {
            THROW_IE_EXCEPTION << "Mean image layout should be CHW";
        }
        _variant = MEAN_IMAGE;
    }

    /**
     * @brief Checks if mean operation is applicable.
     * If applicable, sets the mean type to MEAN_IMAGE for a particular channel
     * @param meanImage Blob with a mean image
     * @param channel Index of a particular channel
     */
    void setMeanImageForChannel(const Blob::Ptr &meanImage, const size_t channel) {
        if (meanImage.get() == nullptr) {
            THROW_IE_EXCEPTION << "Failed to set invalid mean image for channel: nullptr";
        } else if (meanImage.get()->dims().size() != 2) {
            THROW_IE_EXCEPTION << "Failed to set invalid mean image for channel: number of dimensions != 2";
        } else if (channel >= _channelsInfo.size()) {
            THROW_IE_EXCEPTION << "Channel " << channel << " exceed number of PreProcess channels: "
                               << _channelsInfo.size();
        }
        _variant = MEAN_IMAGE;
        _channelsInfo[channel]->meanData = meanImage;
    }

    /**
     * @brief Sets a type of mean operation
     * @param variant Type of mean operation to set
     */
    void setVariant(const MeanVariant &variant) {
        _variant = variant;
    }

    /**
     * @brief Gets a type of mean operation
     * @return The type of mean operation
     */
    MeanVariant getMeanVariant() const {
        return _variant;
    }
};
}  // namespace InferenceEngine
