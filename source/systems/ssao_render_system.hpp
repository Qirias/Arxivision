#pragma once

#include "arx_device.h"
#include "arx_pipeline.h"
#include "arx_descriptors.h"
#include "arx_render_pass_manager.hpp"

// std
#include <vector>

namespace arx {
    class SSAOSystem {
    public:
        SSAOSystem(ArxDevice &device, RenderPassManager& renderPassManager, VkDescriptorSetLayout globalSetLayout);
        ~SSAOSystem();
        
        void renderSSAO(VkCommandBuffer commandBuffer);
    private:
        ArxDevice&                                  device;
        VkRenderPass                                ssaoRenderPass;
        VkPipelineLayout                            pipelineLayout;
        std::unique_ptr<ArxPipeline>                ssaoPipeline;
        std::unique_ptr<ArxDescriptorSetLayout>     descriptorSetLayout;
        VkDescriptorSet                             descriptorSet;
        
        void createDescriptorSetLayout();
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createSSAOPipeline();
    };
}
