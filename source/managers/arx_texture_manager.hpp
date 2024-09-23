#pragma once
#include "../../source/arx_device.h"


namespace arx {

    class Texture {
    public:
        VkImage         image;
        VkDeviceMemory  memory;
        VkImageView     view;
        VkFormat        format;
        VkSampler       sampler = VK_NULL_HANDLE;
        uint32_t        width;
        uint32_t        height;

        void destroy(VkDevice device) {
            vkDestroyImageView(device, view, nullptr);
            vkDestroyImage(device, image, nullptr);
            vkFreeMemory(device, memory, nullptr);
            if (sampler)
                vkDestroySampler(device, sampler, nullptr);
        }
    };

    class FrameBufferAttachment {
    public:
        Texture texture;

        void destroy(VkDevice device) {
            texture.destroy(device);
        }
    };

    class TextureManager {
    public:
        TextureManager(ArxDevice& device);
        ~TextureManager();

        void createTexture2D(
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
        
        // Used for framebuffer since we don't need a sampler
        void createAttachment(const std::string& name, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags);
        // Create stand-alone samplers
        void createSampler(const std::string& name);
        void transitionImageLayout(VkCommandBuffer commandPool, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
        void createTexture2DFromBuffer(
                const std::string& name,
                void* buffer,
                VkDeviceSize bufferSize,
                VkFormat format,
                uint32_t width,
                uint32_t height,
                VkFilter filter = VK_FILTER_LINEAR,
                VkImageUsageFlags imageUsageFlags = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                VkImageLayout imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        
        std::shared_ptr<Texture> getTexture(const std::string& name) const;
        std::shared_ptr<Texture> getAttachment(const std::string& name) const;
        VkSampler getSampler(const std::string& name) const;
        
        void cleanup();
        void cleanAttachments();
        void deleteAttachment(const std::string& name);
        
    private:
        ArxDevice& device;
        std::unordered_map<std::string, std::shared_ptr<Texture>> textures;
        std::unordered_map<std::string, std::shared_ptr<FrameBufferAttachment>> attachments;
        std::unordered_map<std::string, VkSampler> samplers;

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
