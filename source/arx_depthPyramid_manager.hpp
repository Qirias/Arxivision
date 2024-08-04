#pragma once

#include "arx_device.h"
#include <vulkan/vulkan.h>

namespace arx {

    class ArxDepthPyramidManager {
    public:
        ArxDepthPyramidManager(ArxDevice& device, VkExtent2D extent);
        ~ArxDepthPyramidManager();
        
        void createDepthResources(VkExtent2D extent);
        void updateDepthPyramid(VkCommandBuffer cmdBuffer); // Example function to trigger pyramid update
        
        // Getters for shader access, etc.
        VkImageView setDepthImageView() const;
        const std::vector<VkImageView>& getDepthPyramidMipViews() const;
        
        void setDepthImage(VkImage depthImage) { this->depthImage = depthImage; }
        void setDepthSampler(VkSampler depthSampler) { this->depthSampler = depthSampler; }
        
    private:
        ArxDevice&                              device;
        VkImage                                 depthImage;
        VkDeviceMemory                          depthImageMemory;
        VkImageView                             depthImageView;
        
        
        void createDepthImage(VkExtent2D extent);
        void createDepthPyramid(VkExtent2D extent);
        void createDepthSampler();
    };
}
