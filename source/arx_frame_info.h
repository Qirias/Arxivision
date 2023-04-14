#pragma once

#include "arx_camera.h"

namespace arx {
    struct FrameInfo {
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        ArxCamera &camera;
        VkDescriptorSet globalDescriptorSet;
    };
}
