#pragma once

#include "arx_camera.h"
#include "arx_game_object.h"

namespace arx {
    
#define MAX_LIGHTS 10

static const int CHUNK_SIZE = 16;
static const float VOXEL_SIZE = 1;
static const int ADJUSTED_CHUNK = CHUNK_SIZE / VOXEL_SIZE;

    struct PointLight {
      glm::vec4 position{};  // ignore w
      glm::vec4 color{};     // w is intensity
    };

    struct GlobalUbo {
        glm::mat4 projection{1.f};
        glm::mat4 view{1.f};
        glm::mat4 inverseView{1.f};
        glm::vec4 ambientLightColor{1.f, 1.f, 1.f, .02f};
//        PointLight pointLights[MAX_LIGHTS];
        int numLights;
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
