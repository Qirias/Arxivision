#include "arx_texture_manager.hpp"


namespace arx {

    TextureManager::TextureManager(ArxDevice& device) : device(device) {}

    TextureManager::~TextureManager() {
        for (auto& pair : textures) {
            pair.second->destroy(device.device());
        }
        textures.clear();
        
        for (auto& pair : attachments) {
            pair.second->destroy(device.device());
        }
        attachments.clear();
    }

    std::shared_ptr<Texture> TextureManager::createTexture2D(
        const std::string& name,
        uint32_t width,
        uint32_t height,
        VkFormat format,
        VkImageUsageFlags usage,
        VkMemoryPropertyFlags properties,
        uint32_t mipLevels,
        VkSampleCountFlagBits numSamples,
        VkImageTiling tiling,
        VkImageAspectFlags aspectFlags) {

        auto texture = std::make_shared<Texture>();

        createImage(width, height, format, usage, properties, mipLevels, numSamples, tiling, texture->image, texture->memory);
        texture->view = createImageView(texture->image, format, aspectFlags, mipLevels);
        texture->sampler = createSampler();

        textures[name] = texture;
        return texture;
    }

    std::shared_ptr<Texture> TextureManager::getTexture(const std::string& name) const {
        auto it = textures.find(name);
        if (it == textures.end()) {
            throw std::runtime_error("Texture not found: " + name);
        }
        return it->second;
    }


    void TextureManager::createImage(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkImageTiling tiling, VkImage& image, VkDeviceMemory& imageMemory) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width  = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = mipLevels;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = format;
        imageInfo.tiling        = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage         = usage;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.samples       = numSamples;
        
        if (vkCreateImage(device.device(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }
        
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device.device(), image, &memRequirements);
        
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = device.findMemoryType(memRequirements.memoryTypeBits, properties);
        
        if (vkAllocateMemory(device.device(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }
        
        vkBindImageMemory(device.device(), image, imageMemory, 0);
    }

    VkImageView TextureManager::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                              = image;
        viewInfo.viewType                           = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                             = format;
        viewInfo.subresourceRange.aspectMask        = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel      = 0;
        viewInfo.subresourceRange.levelCount        = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer    = 0;
        viewInfo.subresourceRange.layerCount        = 1;

        VkImageView imageView;
        if (vkCreateImageView(device.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }

        return imageView;
    }

    void TextureManager::createAttachment(const std::string& name, uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage) {
        VkImageAspectFlags aspectMask = 0;

        if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT) {
           aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) {
           aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
           if (format >= VK_FORMAT_D16_UNORM_S8_UINT) {
               aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
           }
        }

        assert(aspectMask > 0);

        VkImage image;
        VkDeviceMemory imageMemory;
        createImage(width, height, format, usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, image, imageMemory);

        VkImageView imageView = createImageView(image, format, aspectMask, 1);

        auto attachment = std::make_shared<FrameBufferAttachment>();
        attachment->texture.image = image;
        attachment->texture.memory = imageMemory;
        attachment->texture.view = imageView;
        attachment->texture.format = format;

        attachments[name] = attachment;
    }


    VkSampler TextureManager::createSampler() {
        VkSampler sampler;
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        if (vkCreateSampler(device.device(), &samplerInfo, nullptr, &sampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create color sampler!");
        }
        
        return sampler;
    }
}
