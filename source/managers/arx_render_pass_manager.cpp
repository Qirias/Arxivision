#include "../source/engine_pch.hpp"

#include "../source/managers/arx_render_pass_manager.hpp"


namespace arx {
    RenderPassManager::RenderPassManager(ArxDevice& device) : device(device) {
        
    }

    RenderPassManager::~RenderPassManager() {
        cleanup();
    }

    void RenderPassManager::cleanup() {
        for (auto& pair : renderPasses) {
            vkDestroyRenderPass(device.device(), pair.second, nullptr);
        }
        for (auto& pair : framebuffers) {
            vkDestroyFramebuffer(device.device(), pair.second, nullptr);
        }
        
        renderPasses.clear();
        framebuffers.clear();
    }

    void RenderPassManager::cleanFrameBuffers() {
        for (auto& pair : framebuffers) {
            vkDestroyFramebuffer(device.device(), pair.second, nullptr);
        }
        framebuffers.clear();
    }

    void RenderPassManager::createRenderPass(const std::string& name, const VkRenderPassCreateInfo& createInfo) {
        VkRenderPass renderPass;
        if (vkCreateRenderPass(device.device(), &createInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass: " + name);
        }
        renderPasses[name] = renderPass;
    }

    VkRenderPass RenderPassManager::getRenderPass(const std::string& name) const {
        auto it = renderPasses.find(name);
        if (it == renderPasses.end()) {
            throw std::runtime_error("Render pass not found: " + name);
        }
        return it->second;
    }

    VkFramebuffer RenderPassManager::getFrameBuffer(const std::string& name) const {
        auto it = framebuffers.find(name);
        if (it == framebuffers.end()) {
            throw std::runtime_error("Framebuffer not found: " + name);
        }
        return it->second;
    }
}
