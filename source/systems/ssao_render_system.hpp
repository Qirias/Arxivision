#pragma once

#include "arx_device.h"
#include "arx_pipeline.h"
#include "arx_descriptors.h"
#include "arx_buffer.h"
#include "arx_render_pass_manager.hpp"
#include "arx_texture_manager.hpp"
#include "arx_frame_info.h"

// std
#include <vector>
#include <cassert>

namespace arx {
    class SSAOSystem {
    public:
        SSAOSystem(ArxDevice &device, RenderPassManager& rp, TextureManager& textures, VkExtent2D extent);
        ~SSAOSystem();
        
        void renderSSAO(FrameInfo &frameInfo, bool blurPass = false);
    private:
        ArxDevice&                                  device;
        RenderPassManager&                          rpManager;
        TextureManager&                             textureManager;
        VkExtent2D                                  extent;
        
        // SSAO
        void createSSAOPipelineLayout();
        void createSSAOPipeline();
        
        // SSAO Blur
        void createSSAOBlurPipelineLayout();
        void createSSAOBLurPipeline();
        
        struct {
            std::unique_ptr<ArxPipeline>                ssao;
            std::unique_ptr<ArxPipeline>                ssaoBlur;
        } pipelines;
        
        struct {
            VkPipelineLayout                            ssao;
            VkPipelineLayout                            ssaoBlur;
        } pipelineLayouts;
        
        struct {
            std::unique_ptr<ArxDescriptorSetLayout>     ssao;
            std::unique_ptr<ArxDescriptorSetLayout>     ssaoBlur;
        } descriptorSetLayouts;
        
        struct {
            VkDescriptorSet                             ssao;
            VkDescriptorSet                             ssaoBlur;
        } descriptorSets;
        
        struct {
            std::unique_ptr<ArxBuffer>                  ssaoParams;
            std::unique_ptr<ArxBuffer>                  ssaoKernel;
        } uniformBuffers;
        
        VkSampler colorSampler;
        
        void createDescriptorSetLayout();
        void createRenderPasses();
        
        void createUniformBuffers();
        void createSamplersAndTextures();
        void createFramebuffers();
        void createAttachment(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, FrameBufferAttachment *attachment);
        void beginRenderPass(VkCommandBuffer commandBuffer, bool blurPass = false);
        void endRenderPass(VkCommandBuffer commandBuffer);
    };
}
