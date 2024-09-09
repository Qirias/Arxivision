#pragma once

#include "../source/arx_camera.h"
#include "../source/arx_game_object.h"

namespace arx {
    
static const int CHUNK_SIZE = 16;
static const float VOXEL_SIZE = 0.5;
static const int ADJUSTED_CHUNK = CHUNK_SIZE / VOXEL_SIZE;

static const float SSAO_KERNEL_SIZE = 64;
static const float SSAO_NOISE_DIM = 8;
static const float SSAO_RADIUS = 1.0f;

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

    struct CompositionParams {
        glm::mat4 projection;
        glm::mat4 view;
        glm::mat4 inverseView;
        int32_t ssao = true;
        int32_t ssaoOnly = false;
        int32_t ssaoBlur = true;
        int32_t deferred = true;
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
