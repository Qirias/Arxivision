#pragma once
#include "arx_device.h"

namespace arx {

    class Texture {
    public:
        VkImage         image;
        VkDeviceMemory  memory;
        VkImageView     view;
        VkSampler       sampler;
    };

    class TextureManager {
    public:
        TextureManager(ArxDevice& device);
        ~TextureManager();

        std::shared_ptr<Texture> createTexture2D(
            const std::string& name,
            uint32_t width,
            uint32_t height,
            VkFormat format,
            VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VkMemoryPropertyFlags properties = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
            uint32_t mipLevels = 1,
            VkSampleCountFlagBits numSamples = VK_SAMPLE_COUNT_1_BIT,
            VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL,
            VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT);

        std::shared_ptr<Texture> getTexture(const std::string& name) const;
        void destroyTextures();


    private:
        ArxDevice& device;
        std::unordered_map<std::string, std::shared_ptr<Texture>> textures;

        VkImageView createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels);
        VkSampler createSampler();
        void createImage(
            uint32_t width,
            uint32_t height,
            VkFormat format,
            VkImageUsageFlags usage,
            VkMemoryPropertyFlags properties,
            uint32_t mipLevels,
            VkSampleCountFlagBits numSamples,
            VkImageTiling tiling,
            VkImage& image,
            VkDeviceMemory& imageMemory);
    };
}
