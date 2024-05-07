#pragma once

#include "arx_device.h"
#include "arx_pipeline.h"
#include "arx_descriptors.h"
#include "arx_buffer.h"
#include "arx_render_pass_manager.hpp"
#include "arx_frame_info.h"

// std
#include <vector>
#include <cassert>

namespace arx {
    class SSAOSystem {
    public:
        SSAOSystem(ArxDevice &device, RenderPassManager& renderPassManager, VkExtent2D extent);
        ~SSAOSystem();
        
        void renderSSAO(FrameInfo &frameInfo, bool blurPass = false);
    private:
        ArxDevice&                                  device;
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
        
        struct FrameBufferAttachment {
            VkImage         image;
            VkDeviceMemory  mem;
            VkImageView     view;
            VkFormat        format;
            void destroy(VkDevice device) {
                vkDestroyImage(device, image, nullptr);
                vkDestroyImageView(device, view, nullptr);
                vkFreeMemory(device, mem, nullptr);
            }
        };

        struct FrameBuffer {
            int32_t         width;
            int32_t         height;
            VkFramebuffer   frameBuffer;
            VkRenderPass    renderPass;
            FrameBuffer(VkRenderPass rp = VK_NULL_HANDLE) : renderPass(rp) {}
            
            void setSize(int32_t w, int32_t h) {
                this->width = w;
                this->height = h;
            }
            void destroy(VkDevice device) {
                vkDestroyFramebuffer(device, frameBuffer, nullptr);
                vkDestroyRenderPass(device, renderPass, nullptr);
            }
        };
        
        struct SSAO : public FrameBuffer {
            FrameBufferAttachment color;
            SSAO(VkRenderPass rp = VK_NULL_HANDLE) : FrameBuffer(rp) {}
        } ssao{}, ssaoBlur{};
        
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
