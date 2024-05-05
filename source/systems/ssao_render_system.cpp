#include "ssao_render_system.hpp"


namespace arx {
    SSAOSystem::SSAOSystem(ArxDevice &device, RenderPassManager& renderPassManager, VkDescriptorSetLayout globalSetLayout)
    : device(device), ssaoRenderPass(renderPassManager.getRenderPass("SSAO")) {
        createDescriptorSetLayout();
        createPipelineLayout(globalSetLayout);
        createSSAOPipeline();
    }

    SSAOSystem::~SSAOSystem() {
        vkDestroyPipelineLayout(device.device(), pipelineLayout, nullptr);
    }

    void SSAOSystem::createDescriptorSetLayout() {
        
    }

    void SSAOSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
        
    }

    void SSAOSystem::createSSAOPipeline() {
        
    }

    void SSAOSystem::renderSSAO(VkCommandBuffer commandBuffer) {
        
    }
}
