#include <stdio.h>
#include "app.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <stdexcept>
#include <array>
#include <cassert>

namespace arx {
    
    struct SimplePushConstantData {
        glm::mat2 transform{1.0f};
        glm::vec2 offset;
        alignas(16)glm::vec3 color;
    };

    App::App() {
        loadGameObjects();
        createPipelineLayout();
        recreateSwapChain();
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

    void App::loadGameObjects() {
        std::vector<ArxModel::Vertex> vertices {
            {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}
        };

        auto arxModel = std::make_shared<ArxModel>(arxDevice, vertices);
        
        auto triangle                       = ArxGameObject::createGameObject();
        triangle.model                      = arxModel;
        triangle.color                      = {.1f, .8f, .1f};
        triangle.transform2d.translation.x  = .2f;
        triangle.transform2d.scale          = {2.f, .5f};
        triangle.transform2d.rotation       = .25f * glm::two_pi<float>();
        
        gameObjects.push_back(std::move(triangle));
    }

    void App::createPipelineLayout() {
        
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags    = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset        = 0;
        pushConstantRange.size          = sizeof(SimplePushConstantData);
        
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount           = 0;
        pipelineLayoutInfo.pSetLayouts              = nullptr;
        pipelineLayoutInfo.pushConstantRangeCount   = 1;
        pipelineLayoutInfo.pPushConstantRanges      = &pushConstantRange;
        
        if (vkCreatePipelineLayout(arxDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }

    void App::createPipeline() {
        assert(arxSwapChain != nullptr && "Cannot create pipeline before swap chain");
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");
        
        PipelineConfigInfo pipelineConfig{};
        ArxPipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.renderPass       = arxSwapChain->getRenderPass();
        pipelineConfig.pipelineLayout   = pipelineLayout;
        arxPipeline = std::make_unique<ArxPipeline>(arxDevice,
                                                    "shaders/vert.spv",
                                                    "shaders/frag.spv",
                                                    pipelineConfig);
    }

    void App::freeCommandBuffers() {
        vkFreeCommandBuffers(arxDevice.device(), arxDevice.getCommandPool(), static_cast<uint32_t>(commandBuffers.size()), commandBuffers.data());
        commandBuffers.clear();
    }

    void App::recreateSwapChain() {
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
            arxSwapChain = std::make_unique<ArxSwapChain>(arxDevice, extent, std::move(arxSwapChain));
            if (arxSwapChain->imageCount() != commandBuffers.size()) {
                freeCommandBuffers();
                createCommandBuffers();
            }
        }
        
        // TODO optimization:
        // if render pass is compatible do nothing else recreate it
        createPipeline();
    }

    void App::createCommandBuffers() {
        commandBuffers.resize(arxSwapChain->imageCount());
        
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType                 = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level                 = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool           = arxDevice.getCommandPool();
        allocInfo.commandBufferCount    = static_cast<uint32_t>(commandBuffers.size());
        
        if (vkAllocateCommandBuffers(arxDevice.device(), &allocInfo, commandBuffers.data()) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate command buffers");
        }
    }
    
    void App::recordCommandBuffer(int imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType     = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        
        if (vkBeginCommandBuffer(commandBuffers[imageIndex], &beginInfo) != VK_SUCCESS) {
            throw std::runtime_error("failed to begin recording commnad buffer!");
        }
        
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType        = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass   = arxSwapChain->getRenderPass();
        renderPassInfo.framebuffer  = arxSwapChain->getFrameBuffer(imageIndex);
        
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = arxSwapChain->getSwapChainExtent();
        
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color            = {0.01f, 0.01f, 0.01f, 1.0f};
        clearValues[1].depthStencil     = {1.0f, 0};
        renderPassInfo.clearValueCount  = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues     = clearValues.data();
        
        vkCmdBeginRenderPass(commandBuffers[imageIndex], &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        VkViewport viewport{};
        viewport.x  = 0.0f;
        viewport.y  = 0.0f;
        viewport.width  = static_cast<float>(arxSwapChain->getSwapChainExtent().width);
        viewport.height = static_cast<float>(arxSwapChain->getSwapChainExtent().height);
        viewport.minDepth   = 0.0f;
        viewport.maxDepth   = 1.0f;
        VkRect2D scissor{{0, 0}, arxSwapChain->getSwapChainExtent()};
        vkCmdSetViewport(commandBuffers[imageIndex], 0, 1, &viewport);
        vkCmdSetScissor(commandBuffers[imageIndex], 0, 1, &scissor);
        
        renderGameObjects(commandBuffers[imageIndex]);

        vkCmdEndRenderPass(commandBuffers[imageIndex]);
        if (vkEndCommandBuffer(commandBuffers[imageIndex]) != VK_SUCCESS) {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void App::renderGameObjects(VkCommandBuffer commandBuffer) {
        arxPipeline->bind(commandBuffer);
        
        for (auto& obj : gameObjects) {
            obj.transform2d.rotation = glm::mod(obj.transform2d.rotation + 0.01f, glm::two_pi<float>());
            
            SimplePushConstantData push{};
            push.offset     = obj.transform2d.translation;
            push.color      = obj.color;
            push.transform  = obj.transform2d.mat2();
            
            vkCmdPushConstants(commandBuffer,
                               pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT |
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(SimplePushConstantData),
                               &push);
            
            obj.model->bind(commandBuffer);
            obj.model->draw(commandBuffer);
        }
    }

    void App::drawFrame() {
        uint32_t imageIndex;
        auto result = arxSwapChain->acquireNextImage(&imageIndex);
        
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            recreateSwapChain();
            return;
        }
        
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
            throw std::runtime_error("failed to acquire swap chain image!");
        }
        
        recordCommandBuffer(imageIndex);
        result = arxSwapChain->submitCommandBuffers(&commandBuffers[imageIndex], &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || arxWindow.wasWindowResized()) {
            arxWindow.resetWindowResizedFlag();
            recreateSwapChain();
            return;
        }
        if (result != VK_SUCCESS) {
            throw std::runtime_error("failed to present swap chain image!");
        }
    }
}
