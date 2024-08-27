#pragma once

#include "arx_window.h"
#include "arx_device.h"
#include "arx_swap_chain.h"
#include "arx_frame_info.h"
#include "arx_render_pass_manager.hpp"
#include "arx_texture_manager.hpp"
#include "chunks.h"
#include "systems/clustered_shading_system.hpp"

// std
#include <memory>
#include <vector>
#include <cassert>

namespace arx {

    class ArxRenderer {
    public:
        
        ArxRenderer(ArxWindow &window, ArxDevice &device, RenderPassManager &rp, TextureManager &textures);
        ~ArxRenderer();
        
        ArxRenderer(const ArxRenderer &) = delete;
        ArxRenderer &operator=(const ArxRenderer&) = delete;
        
        VkRenderPass getSwapChainRenderPass() const { return arxSwapChain->getRenderPass(); }
        float getAspectRatio() const { return arxSwapChain->extentAspectRatio(); }
        bool isFrameInProgress() const { return isFrameStarted; }
        
        VkCommandBuffer getCurrentCommandBuffer() const {
            assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
            return commandBuffers[currentFrameIndex];
        }
        
        int getFrameIndex() const {
            assert(isFrameStarted && "Cannot get frame index when frame not in progress");
            return currentFrameIndex;
        }
        
        ArxSwapChain* getSwapChain() const { return arxSwapChain.get(); }
        
        VkCommandBuffer beginFrame();
        void endFrame();
        
        void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
        void beginRenderPass(VkCommandBuffer commandBuffer, const std::string& name);
        
        void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

        
        // Passes
        void Passes(FrameInfo &frameInfo);
        void init_Passes();
        
        void updateUnirofms(const GlobalUbo &rhs, const CompositionParams &ssaorhs);
        void cleanupResources();
    private:
        void createCommandBuffers();
        void freeCommandBuffers();
        void recreateSwapChain();
        
        // Resources
        void createRenderPasses();
        void createDescriptorSetLayouts();
        void createPipelineLayouts();
        void createPipelines();
        void createUniformBuffers();
        
        ArxWindow&                      arxWindow;
        ArxDevice&                      arxDevice;
        
        RenderPassManager&              rpManager;
        TextureManager&                 textureManager;
        
        std::unique_ptr<ArxSwapChain>   arxSwapChain;
        std::vector<VkCommandBuffer>    commandBuffers;
        
        uint32_t                        currentImageIndex;
        int                             currentFrameIndex{0};
        bool                            isFrameStarted = false;
        
        GlobalUbo                       ubo{};
        CompositionParams               compParams{};
    };
}
