#include "arx_render_pass_manager.hpp"

namespace arx {
    RenderPassManager::RenderPassManager(ArxDevice& device) : device(device) {
        
    }

    RenderPassManager::~RenderPassManager() {
        destroyRenderPasses();
    }

    VkRenderPass RenderPassManager::createRenderPass(const std::string& name, const VkRenderPassCreateInfo& createInfo) {
        VkRenderPass renderPass;
        if (vkCreateRenderPass(device.device(), &createInfo, nullptr, &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create render pass: " + name);
        }
        renderPasses[name] = renderPass;
        return renderPass;
    }

    VkRenderPass RenderPassManager::getRenderPass(const std::string& name) const {
        auto it = renderPasses.find(name);
        if (it == renderPasses.end()) {
            throw std::runtime_error("Render pass not found: " + name);
        }
        return it->second;
    }

    void RenderPassManager::destroyRenderPasses() {
        for (auto& pair : renderPasses) {
            vkDestroyRenderPass(device.device(), pair.second, nullptr);
        }
        renderPasses.clear();
    }


    // Example usage
//    VkRenderPassCreateInfo ssaoPassInfo = {/* Fill with SSAO-specific setup */};
//    renderPassManager.createRenderPass("ssao", ssaoPassInfo);
}
