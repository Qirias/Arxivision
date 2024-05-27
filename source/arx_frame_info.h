#pragma once

#include "arx_camera.h"
#include "arx_game_object.h"

namespace arx {
    
static const int CHUNK_SIZE = 9;
static const float VOXEL_SIZE = 1;
static const int ADJUSTED_CHUNK = CHUNK_SIZE / VOXEL_SIZE;

    struct AABB {
        glm::vec3 min{};
        glm::vec3 max{};
    };

    struct GlobalUbo {
        glm::mat4 projection{1.f};
        glm::mat4 view{1.f};
        glm::mat4 inverseView{1.f};
        float zNear{.1f};
        float zFar{1024.f};
    };
    
    struct FrameInfo {
        int frameIndex;
        float frameTime;
        VkCommandBuffer commandBuffer;
        ArxCamera &camera;
        VkDescriptorSet globalDescriptorSet;
        ArxGameObject::Map &voxel;
    };
}
