#pragma once

#include "arx_window.h"
#include "arx_device.h"
#include "arx_swap_chain.h"
#include "arx_frame_info.h"
#include "chunks.h"

// std
#include <memory>
#include <vector>
#include <cassert>

namespace arx {

    class ArxRenderer {
    public:
        
        ArxRenderer(ArxWindow &window, ArxDevice &device);
        ~ArxRenderer();
        
        ArxRenderer(const ArxRenderer &) = delete;
        ArxRenderer &operator=(const ArxRenderer&) = delete;
        
        VkRenderPass getSwapChainRenderPass() const { return arxSwapChain->getRenderPass(); }
        float getAspectRation() const { return arxSwapChain->extentAspectRatio(); }
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
        void beginSwapChainRenderPass(FrameInfo &frameInfo, VkCommandBuffer commandBuffer);
        void endSwapChainRenderPass(VkCommandBuffer commandBuffer);

        void beginLateRenderPass(FrameInfo &frameInfo, VkCommandBuffer commandBuffer);
        void endLateRenderPass(VkCommandBuffer commandBuffer);
        
    private:
        void createCommandBuffers();
        void freeCommandBuffers();
        void recreateSwapChain();
        
        ArxWindow&                      arxWindow;
        ArxDevice&                      arxDevice;
        std::unique_ptr<ArxSwapChain>   arxSwapChain;
        std::vector<VkCommandBuffer>    commandBuffers; 
        
        uint32_t                        currentImageIndex;
        int                             currentFrameIndex{0};
        bool                            isFrameStarted = false;
    };
}
