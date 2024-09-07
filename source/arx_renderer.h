#pragma once

#include "../source/arx_window.h"
#include "../source/arx_device.h"
#include "../source/arx_swap_chain.h"
#include "../source/managers/arx_render_pass_manager.hpp"
#include "../source/managers/arx_texture_manager.hpp"

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
        
        void updateUniforms(const GlobalUbo &rhs, const CompositionParams &ssaorhs);
        void cleanupResources();
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities, VkExtent2D windowExtent);
    private:
        void createCommandBuffers();
        void freeCommandBuffers();
        void recreateSwapChain();
        
        // Resources
        void createRenderPasses();
        void createFramebuffers();
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
