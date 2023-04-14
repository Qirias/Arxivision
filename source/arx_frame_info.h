#pragma once

#include "arx_camera.h"
#include "arx_game_object.h"

namespace arx {
    struct FrameInfo {
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        ArxCamera &camera;
        VkDescriptorSet globalDescriptorSet;
        ArxGameObject::Map &gameObjects;
    };
}
