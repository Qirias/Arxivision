#pragma once

#include "../source/arx_device.h"
#include "../source/systems/occlusion_system.hpp"
#include "../source/managers/arx_render_pass_manager.hpp"
#include "../source/managers/arx_texture_manager.hpp"

namespace arx {

class ArxSwapChain {
    public:
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;

        ArxSwapChain(ArxDevice &deviceRef, VkExtent2D windowExtent, RenderPassManager &rp, TextureManager &textures);
        ArxSwapChain(ArxDevice &deviceRef, VkExtent2D windowExtent, std::shared_ptr<ArxSwapChain> previous, RenderPassManager &rp, TextureManager &textures);
    
        ~ArxSwapChain();

        ArxSwapChain(const ArxSwapChain &) = delete;
        ArxSwapChain& operator=(const ArxSwapChain &) = delete;

        VkFramebuffer getFrameBuffer(int index) { return swapChainFramebuffers[index]; }
        VkFramebuffer getEarlyFrameBuffer(int index) { return earlySwapChainFramebuffers[index]; }
        VkRenderPass getRenderPass() { return renderPass; }
        VkRenderPass getEarlyRenderPass() { return earlyRenderPass; }
        VkImageView getImageView(int index) { return swapChainImageViews[index]; }
        size_t imageCount() { return swapChainImages.size(); }
        VkFormat getSwapChainImageFormat() { return swapChainImageFormat; }
        VkExtent2D getSwapChainExtent() { return swapChainExtent; }
        uint32_t width() { return swapChainExtent.width; }
        uint32_t height() { return swapChainExtent.height; }
    
        void Init_OcclusionCulling();

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
        void createRenderPass(bool earlyPass = false);
        void createFramebuffers(bool earlyPass = false);
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
    
        std::vector<VkFramebuffer>  earlySwapChainFramebuffers;
        VkRenderPass                earlyRenderPass;

        
        std::vector<VkImage>        depthImages; // Multisampled
        std::vector<VkDeviceMemory> depthImageMemorys;
        std::vector<VkImageView>    depthImageViews;
    
        // Single-sampled depth resolve sampler
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
    
        RenderPassManager&              rpManager;
        TextureManager&                 textureManager;        
    
    public:
        OcclusionSystem cull;
        void createDepthPyramid();
        void createDepthSampler();
        void createDepthPyramidDescriptors();
        void computeDepthPyramid(VkCommandBuffer commandBuffer);
        void createBarriers();
        void loadGeometryToDevice();
        VkImageMemoryBarrier createImageBarrier(VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image, VkImageAspectFlags aspectFlags, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, uint32_t baseMipLevels, uint32_t levelCount);
    
        void createCullingDescriptors();
        void updateDynamicData();
        void computeCulling(VkCommandBuffer commandBuffer, const uint32_t chunkCount, bool early = false);
    };
}
