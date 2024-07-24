#pragma once

#include "arx_device.h"
#include "arx_pipeline.h"
#include "arx_descriptors.h"
#include "arx_buffer_manager.hpp"
#include "arx_model.h"
#include "arx_frame_info.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

namespace arx {

    class FaceVisibilitySystem {
    public:
        static void init(ArxDevice &device);
        static void cleanup();

        static void removeHiddenFrontFaces(VkCommandBuffer commandBuffer);
        
        static VkPipelineLayout getPipelineLayout() { return pipelineLayout; }
        static VkPipeline getPipeline() { return pipeline->computePipeline; }
        static VkDescriptorSet getDescriptorSet() { return descriptorSet; }

    private:
        FaceVisibilitySystem() = delete;
        ~FaceVisibilitySystem() = delete;
        FaceVisibilitySystem(const FaceVisibilitySystem &) = delete;
        FaceVisibilitySystem &operator=(const FaceVisibilitySystem &) = delete;

        static void createDescriptorSetLayout();
        static void createPipelineLayout();
        static void createPipeline();
        static void createDescriptorPool();
        static void createDescriptorSets();
        
        static ArxDevice* arxDevice;

        static std::unique_ptr<ArxDescriptorSetLayout> descriptorSetLayout;
        static VkPipelineLayout pipelineLayout;
        static std::unique_ptr<ArxPipeline> pipeline;
        static std::unique_ptr<ArxDescriptorPool> descriptorPool;
        static VkDescriptorSet descriptorSet;

        static std::shared_ptr<ArxBuffer> largeInstanceBuffer;
        static std::shared_ptr<ArxBuffer> faceVisibilityBuffer;

        struct WorldDimensionsData {
            glm::ivec3 worldDimensions;
        };
    };
}
