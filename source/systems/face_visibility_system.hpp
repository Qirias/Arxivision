#pragma once

#include "../source/arx_device.h"
#include "../source/arx_pipeline.h"
#include "../source/arx_descriptors.h"
#include "../source/managers/arx_buffer_manager.hpp"

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
