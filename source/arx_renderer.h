#pragma once

#include "arx_window.h"
#include "arx_device.h"
#include "arx_swap_chain.h"

// std
#include <memory>
#include <vector>
#include <cassert>

namespace arx {

    class ArxRenderer {
    public:
        
        ArxRenderer(ArxWindow &window, ArxDevice &device);
        ~ArxRenderer();
        
        ArxRenderer(const ArxWindow &) = delete;
        ArxRenderer &operator=(const ArxWindow &) = delete;
        
        VkRenderPass getSwapChainRenderPass() const { return arxSwapChain->getRenderPass(); }
        bool isFrameInProgress() const { return isFrameStarted; }
        
        VkCommandBuffer getCurrentCommandBuffer() const {
            assert(isFrameStarted && "Cannot get command buffer when frame not in progress");
            return commandBuffers[currentFrameIndex];
        }
        
        int getFrameIndex() const {
            assert(isFrameStarted && "Cannot get frame index when frame not in progress");
            return currentFrameIndex;
        }
        
        VkCommandBuffer beginFrame();
        void endFrame();
        void beginSwapChainRenderPass(VkCommandBuffer commandBuffer);
        void endSwapChainRenderPass(VkCommandBuffer commandBuffer);
        
    private:
        void createCommandBuffers();
        void freeCommandBuffers();
        void recreateSwapChain();
        
        ArxWindow&                      arxWindow;
        ArxDevice&                      arxDevice;
        std::unique_ptr<ArxSwapChain>   arxSwapChain;
        std::vector<VkCommandBuffer>    commandBuffers;
        
        uint32_t                        currentImageIndex;
        int                             currentFrameIndex;
        bool                            isFrameStarted = false;
    };
}
