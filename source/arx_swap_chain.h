#pragma once

#include "arx_device.h"
#include "systems/occlusion_system.hpp"

// std lib headers
#include <string>
#include <vector>
#include <memory>

namespace arx {

class ArxSwapChain {
    public:
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

        ArxSwapChain(ArxDevice &deviceRef, VkExtent2D windowExtent);
        ArxSwapChain(ArxDevice &deviceRef, VkExtent2D windowExtent, std::shared_ptr<ArxSwapChain> previous);
    
        ~ArxSwapChain();

        ArxSwapChain(const ArxSwapChain &) = delete;
        ArxSwapChain& operator=(const ArxSwapChain &) = delete;

        VkFramebuffer getFrameBuffer(int index) { return swapChainFramebuffers[index]; }
        VkFramebuffer getLateFrameBuffer(int index) { return lateSwapChainFramebuffers[index]; }
        VkRenderPass getRenderPass() { return renderPass; }
        VkRenderPass getLateRenderPass() { return lateRenderPass; }
        VkImageView getImageView(int index) { return swapChainImageViews[index]; }
        VkImage getDepthImage() { return depthImage; }
        size_t imageCount() { return swapChainImages.size(); }
        VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }
        VkExtent2D getSwapChainExtent() { return swapChainExtent; }
        uint32_t width() { return swapChainExtent.width; }
        uint32_t height() { return swapChainExtent.height; }

        float extentAspectRatio() {
            return static_cast<float>(swapChainExtent.width) / static_cast<float>(swapChainExtent.height);
        }
        VkFormat findDepthFormat();

        VkResult acquireNextImage(uint32_t *imageIndex);
        VkResult submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex);
    
        bool compareSwapFormats(const ArxSwapChain &swapChain) const {
            return swapChain.swapChainDepthFormat == swapChainDepthFormat &&
                   swapChain.swapChainImageFormat == swapChainImageFormat;
        }

    private:
        void init();
        void createSwapChain();
        void createImageViews();
        void createDepthResources();
        void createRenderPass(bool latePass = false);
        void createFramebuffers(bool latePass = false);
        void createSyncObjects();
        void createColorResources();
        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, uint32_t baseMipLevel = 0);
        void createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory);

        // Helper functions
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities);

        VkFormat                                        swapChainImageFormat;
        VkFormat                                        swapChainDepthFormat;
        VkExtent2D                                      swapChainExtent;
    
        VkImage                                         colorImage;
        VkDeviceMemory                                  colorImageMemory;
        VkImageView                                     colorImageView;
        VkPhysicalDeviceImagelessFramebufferFeatures    imagelessFramebufferFeatures;


        std::vector<VkFramebuffer>  swapChainFramebuffers;
        VkRenderPass                renderPass;
    
        std::vector<VkFramebuffer>  lateSwapChainFramebuffers;
        VkRenderPass                lateRenderPass;

        
        std::vector<VkImage>        depthImages; // Multisampled
        std::vector<VkDeviceMemory> depthImageMemorys;
        std::vector<VkImageView>    depthImageViews;
    
        // Single-sampled depth resolve attachment
        VkImage                     depthImage;
        VkImageView                 depthImageView;
        VkDeviceMemory              depthImageMemory;
        VkSampler                   depthSampler;
    
        std::vector<VkImage>        swapChainImages;
        std::vector<VkImageView>    swapChainImageViews;

        ArxDevice &device;
        VkExtent2D windowExtent;

        VkSwapchainKHR                  swapChain;
        std::shared_ptr<ArxSwapChain>   oldSwapChain;

        std::vector<VkSemaphore>        imageAvailableSemaphores;
        std::vector<VkSemaphore>        renderFinishedSemaphores;
        std::vector<VkFence>            inFlightFences;
        std::vector<VkFence>            imagesInFlight;
        size_t                          currentFrame = 0;
    
    public:
        OcclusionSystem cull;
        void createDepthPyramid();
        void createDepthSampler();
        void createDepthPyramidDescriptors();
        void computeDepthPyramid(VkCommandBuffer commandBuffer);
        void createBarriers();
        void loadGeometryToDevice();
        VkBufferMemoryBarrier bufferBarrier(VkBuffer buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
        VkImageMemoryBarrier createImageBarrier(VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image, VkImageAspectFlags aspectFlags, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, uint32_t baseMipLevels, uint32_t levelCount);
    
        void createCullingDescriptors();
        void updateDynamicData();
        std::vector<uint32_t> computeCulling(VkCommandBuffer commandBuffer, const uint32_t instances, bool late = false);
    };
}
