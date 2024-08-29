#pragma once

#include "../../source/arx_device.h"

namespace arx {

    class RenderPassManager {
    public:
        RenderPassManager(ArxDevice& device);
        ~RenderPassManager();
        
        template <typename T, size_t N>
        void createFramebuffer(const std::string& renderPassName, const std::array<T, N>& attachmentViews, uint32_t width, uint32_t height) {
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = getRenderPass(renderPassName);
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachmentViews.size());
            framebufferInfo.pAttachments = attachmentViews.data();
            framebufferInfo.width = width;
            framebufferInfo.height = height;
            framebufferInfo.layers = 1;

            VkFramebuffer framebuffer;
            if (vkCreateFramebuffer(device.device(), &framebufferInfo, nullptr, &framebuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create framebuffer for render pass: " + renderPassName);
            }
            framebuffers[renderPassName] = std::move(framebuffer);
        }
        void createRenderPass(const std::string& name, const VkRenderPassCreateInfo& createInfo);
        
        VkFramebuffer getFrameBuffer(const std::string& name) const;
        VkRenderPass getRenderPass(const std::string& name) const;
    private:
        ArxDevice& device;
        std::unordered_map<std::string, VkRenderPass> renderPasses;
        std::unordered_map<std::string, VkFramebuffer> framebuffers;
    };
}
