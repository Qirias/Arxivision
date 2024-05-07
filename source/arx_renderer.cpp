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
            arxSwapChain = std::make_unique<ArxSwapChain>(arxDevice, extent);
        }
        else {
            std::shared_ptr<ArxSwapChain> oldSwapChain = std::move(arxSwapChain);
            arxSwapChain = std::make_unique<ArxSwapChain>(arxDevice, extent, oldSwapChain);
            
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

    void ArxRenderer::beginSwapChainRenderPass(FrameInfo &frameInfo, VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "Can't beginSwapChainRenderPass if frame is not in progress");
        assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame");
        
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType        = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass   = arxSwapChain->getRenderPass();
        renderPassInfo.framebuffer  = arxSwapChain->getFrameBuffer(currentImageIndex);
        
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = arxSwapChain->getSwapChainExtent();
        
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color            = {0.6f, 0.6f, 0.6f, 1.0f};
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

    void ArxRenderer::endSwapChainRenderPass(VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "Can't endSwapChainRenderPass if frame is not in progress");
        assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame");
        
        vkCmdEndRenderPass(commandBuffer);
    }

    void ArxRenderer::beginLateRenderPass(FrameInfo &frameInfo, VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "Can't beginLateRenderPass if frame is not in progress");
        assert(commandBuffer == getCurrentCommandBuffer() && "Can't begin render pass on command buffer from a different frame");
        
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType        = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass   = arxSwapChain->getLateRenderPass();
        renderPassInfo.framebuffer  = arxSwapChain->getLateFrameBuffer(currentImageIndex);
        
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = arxSwapChain->getSwapChainExtent();
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    }

    void ArxRenderer::endLateRenderPass(VkCommandBuffer commandBuffer) {
        assert(isFrameStarted && "Can't endLateRenderPass if frame is not in progress");
        assert(commandBuffer == getCurrentCommandBuffer() && "Can't end render pass on command buffer from a different frame");
        
        vkCmdEndRenderPass(commandBuffer);
    }

    void ArxRenderer::createGBufferRenderPass() {
        // attachments
        VkFormat depthFormat = arxSwapChain->findDepthFormat();
        
        textureManager.createAttachment("gPosDepth", arxSwapChain->width(), arxSwapChain->height(), 
                                        VK_FORMAT_R32G32B32A32_SFLOAT,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        textureManager.createAttachment("gNormals", arxSwapChain->width(), arxSwapChain->height(), 
                                        VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        textureManager.createAttachment("gAlbedo", arxSwapChain->width(), arxSwapChain->height(),
                                        VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        textureManager.createAttachment("gDepth", arxSwapChain->width(), arxSwapChain->height(), 
                                        depthFormat,
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        
        std::array<VkAttachmentDescription, 4> attachmentDescs = {};
        for (uint32_t i = 0; i < static_cast<uint32_t>(attachmentDescs.size()); i++) {
            attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
            attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescs[i].finalLayout = (i == 3) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }

        // Formats
        attachmentDescs[0].format = textureManager.getTexture("gPosDepth")->format;
        attachmentDescs[1].format = textureManager.getTexture("gNormals")->format;
        attachmentDescs[2].format = textureManager.getTexture("gAlbedo")->format;
        attachmentDescs[3].format = textureManager.getTexture("gDepth")->format;
        
        std::vector<VkAttachmentReference> colorReferences;
        colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
        colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
        colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });

        VkAttachmentReference depthReference = {};
        depthReference.attachment = 3;
        depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pColorAttachments = colorReferences.data();
        subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
        subpass.pDepthStencilAttachment = &depthReference;

        // Use subpass dependencies for attachment layout transitions
        std::array<VkSubpassDependency, 2> dependencies;

        dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[0].dstSubpass = 0;
        dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        dependencies[1].srcSubpass = 0;
        dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        VkRenderPassCreateInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.pAttachments = attachmentDescs.data();
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 2;
        renderPassInfo.pDependencies = dependencies.data();
        
        rpManager.createRenderPass("GBuffer", renderPassInfo);

        std::array<VkImageView, 4> attachments;
        attachments[0] = textureManager.getTexture("gPosDepth")->view;
        attachments[1] = textureManager.getTexture("gNormals")->view;
        attachments[2] = textureManager.getTexture("gAlbedo")->view;
        attachments[3] = textureManager.getTexture("gDepth")->view;

        rpManager.createFramebuffer("GBuffer", attachments, arxSwapChain->width(), arxSwapChain->height());
    }
}
