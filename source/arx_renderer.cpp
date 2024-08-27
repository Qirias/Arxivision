#include <stdio.h>
#include "arx_renderer.h"


// std
#include <stdexcept>
#include <array>
#include <cassert>

namespace arx {

    ArxRenderer::ArxRenderer(ArxWindow &window, ArxDevice &device, RenderPassManager &rp, TextureManager &textures)
    : arxWindow{window}, arxDevice{device}, rpManager{rp}, textureManager{textures} {
        recreateSwapChain();
        createCommandBuffers();
    }

    ArxRenderer::~ArxRenderer() {
        cleanupResources();
        freeCommandBuffers();
    }

    void ArxRenderer::freeCommandBuffers() {
        vkFreeCommandBuffers(arxDevice.device(),
                             arxDevice.getCommandPool(),
                             static_cast<uint32_t>(commandBuffers.size()),
                             commandBuffers.data());
        commandBuffers.clear();
    }

    void ArxRenderer::recreateSwapChain() {
        auto extent = arxWindow.getExtend();
        while (extent.width == 0 || extent.height == 0) {
            extent = arxWindow.getExtend();
            glfwWaitEvents();
        }
        
        vkDeviceWaitIdle(arxDevice.device());
        
        if (arxSwapChain == nullptr) {
            arxSwapChain = std::make_unique<ArxSwapChain>(arxDevice, extent, rpManager, textureManager);
        }
        else {
            std::shared_ptr<ArxSwapChain> oldSwapChain = std::move(arxSwapChain);
            arxSwapChain = std::make_unique<ArxSwapChain>(arxDevice, extent, oldSwapChain, rpManager, textureManager);
            
            if (!oldSwapChain->compareSwapFormats(*arxSwapChain.get())) {
                throw std::runtime_error("Swap chain image(or depth) format has changed");
            }
        }
    }

    void ArxRenderer::createCommandBuffers() {
        commandBuffers.resize(ArxSwapChain::MAX_FRAMES_IN_FLIGHT);
        
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType                 = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level                 = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool           = arxDevice.getCommandPool();
        allocInfo.commandBufferCount    = static_cast<uint32_t>(commandBuffers.size());
        
        if (vkAllocateCommandBuffers(arxDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers");
        }
    }

    VkCommandBuffer ArxRenderer::beginFrame() {
        assert(!isFrameStarted && "Can't call beginFrame while already in progress");
        
        auto result = arxSwapChain->acquireNextImage(&currentImageIndex);
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return nullptr;
        }
        
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        
        isFrameStarted = true;
        
        auto commandBuffer = getCurrentCommandBuffer();
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording commnad buffer!");
        }
        
        return commandBuffer;
    }

    void ArxRenderer::endFrame() {
        assert(isFrameStarted && "Can't call endFrame while frame is not in progress");
        
        auto commandBuffer = getCurrentCommandBuffer();
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
        auto result = arxSwapChain->submitCommandBuffers(&commandBuffer, &currentImageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || arxWindow.wasWindowResized()) {
            arxWindow.resetWindowResizedFlag();
            recreateSwapChain();
        } else if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
        
        isFrameStarted = false;
        currentFrameIndex = (currentFrameIndex + 1) % ArxSwapChain::MAX_FRAMES_IN_FLIGHT;
    }

    void ArxRenderer::beginSwapChainRenderPass(VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "Can't beginSwapChainRenderPass if frame is not in progress");
        assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame");
        
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType        = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass   = arxSwapChain->getRenderPass();
        renderPassInfo.framebuffer  = arxSwapChain->getFrameBuffer(currentImageIndex);
        
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = arxSwapChain->getSwapChainExtent();
        
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color            = {0.0f, 0.0f, 0.0f, 1.0f};
        clearValues[1].depthStencil     = {1.0f, 0};
        renderPassInfo.clearValueCount  = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues     = clearValues.data();
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        VkViewport viewport{};
        viewport.x  = 0.0f;
        viewport.y  = 0.0f;
        viewport.width  = static_cast<float>(arxSwapChain->getSwapChainExtent().width);
        viewport.height = static_cast<float>(arxSwapChain->getSwapChainExtent().height);
        viewport.minDepth   = 0.0f;
        viewport.maxDepth   = 1.0f;
        VkRect2D scissor{{0, 0}, arxSwapChain->getSwapChainExtent()};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    void ArxRenderer::beginRenderPass(VkCommandBuffer commandBuffer, const std::string& name) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType        = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass   = rpManager.getRenderPass(name);
        renderPassInfo.framebuffer  = rpManager.getFrameBuffer(name);
        
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = arxSwapChain->getSwapChainExtent();
        
        std::vector<VkClearValue> clearValues;
        
        if (name == "GBuffer") {
            clearValues.resize(4);
            clearValues[0].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
            clearValues[1].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
            clearValues[2].color = { { 0.0f, 0.0f, 0.0f, 1.0f } };
            clearValues[3].depthStencil = { 1.0f, 0 };
            renderPassInfo.clearValueCount  = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues     = clearValues.data();
        }
        else {
            clearValues.resize(2);
            clearValues[0].color            = {0.0f, 0.0f, 0.0f, 1.0f};
            clearValues[1].depthStencil     = {1.0f, 0};
            renderPassInfo.clearValueCount  = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues     = clearValues.data();
        }

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        VkViewport viewport{};
        viewport.x  = 0.0f;
        viewport.y  = 0.0f;
        viewport.width  = static_cast<float>(arxSwapChain->getSwapChainExtent().width);
        viewport.height = static_cast<float>(arxSwapChain->getSwapChainExtent().height);
        viewport.minDepth   = 0.0f;
        viewport.maxDepth   = 1.0f;
        VkRect2D scissor{{0, 0}, arxSwapChain->getSwapChainExtent()};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    void ArxRenderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "Can't endSwapChainRenderPass if frame is not in progress");
        assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame");
        
        vkCmdEndRenderPass(commandBuffer);
    }
}
