//
//  app.cpp
//  ArXivision
//
//  Created by kiri on 26/3/23.
//

#include <stdio.h>
#include "app.h"

// std
#include <stdexcept>
#include <array>

namespace arx {
    
    App::App() {
        createPipelineLayout();
        createPipeline();
        createCommandBuffers();
    }

    App::~App() {
        vkDestroyPipelineLayout(arxDevice.device(), pipelineLayout, nullptr);
    }
    
    void App::run() {
        
        while (!arxWindow.shouldClose()) {
            glfwPollEvents();
            drawFrame();
        }
        
        vkDeviceWaitIdle(arxDevice.device());
    }

    void App::createPipelineLayout() {
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount           = 0;
        pipelineLayoutInfo.pSetLayouts              = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount   = 0;
        pipelineLayoutInfo.pPushConstantRanges      = nullptr;
        
        if (vkCreatePipelineLayout(arxDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }

    void App::createPipeline() {
        auto pipelineConfig = ArxPipeline::defaultPipelineConfigInfo(arxSwapChain.width(), arxSwapChain.height());
        pipelineConfig.renderPass   = arxSwapChain.getRenderPass();
        pipelineConfig.pipelineLayout = pipelineLayout;
        arxPipeline = std::make_unique<ArxPipeline>(arxDevice,
                                                    "shaders/vert.spv",
                                                    "shaders/frag.spv",
                                                    pipelineConfig);
    }

    void App::createCommandBuffers() {
        commandBuffers.resize(arxSwapChain.imageCount());
        
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType                 = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level                 = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool           = arxDevice.getCommandPool();
        allocInfo.commandBufferCount    = static_cast<uint32_t>(commandBuffers.size());
        
        if (vkAllocateCommandBuffers(arxDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers");
        }
        
        for (int i = 0; i < commandBuffers.size(); i++) {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            
            if (vkBeginCommandBuffer(commandBuffers[i], &beginInfo) != VK_SUCCESS) {
                throw std::runtime_error("failed to begin recording commnad buffer!");
            }
            
            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType        = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass   = arxSwapChain.getRenderPass();
            renderPassInfo.framebuffer  = arxSwapChain.getFrameBuffer(i);
            
            renderPassInfo.renderArea.offset = {0, 0};
            renderPassInfo.renderArea.extent = arxSwapChain.getSwapChainExtent();
            
            std::array<VkClearValue, 2> clearValues{};
            clearValues[0].color            = {0.1f, 0.1f, 0.1f, 1.0f};
            clearValues[1].depthStencil     = {1.0f, 0};
            renderPassInfo.clearValueCount  = static_cast<uint32_t>(clearValues.size());
            renderPassInfo.pClearValues     = clearValues.data();
            
            vkCmdBeginRenderPass(commandBuffers[i], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            
            arxPipeline->bind(commandBuffers[i]);
            vkCmdDraw(commandBuffers[i], 3, 1, 0, 0);
            
            vkCmdEndRenderPass(commandBuffers[i]);
            if (vkEndCommandBuffer(commandBuffers[i]) != VK_SUCCESS) {
                throw std::runtime_error("failed to record command buffer!");
            }
        }
    }
    void App::drawFrame() {
        uint32_t imageIndex;
        auto result = arxSwapChain.acquireNextImage(&imageIndex);
        
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        
        result = arxSwapChain.submitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
    }
}
