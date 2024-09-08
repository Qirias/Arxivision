#include "../source/engine_pch.hpp"

#include "../source/managers/arx_texture_manager.hpp"

namespace arx {

    TextureManager::TextureManager(ArxDevice& device) : device(device) {}

    TextureManager::~TextureManager() {
        cleanup();
    }

    void TextureManager::cleanup() {
        for (auto& pair : textures) {
            pair.second->destroy(device.device());
        }
        textures.clear();
        
        for (auto& pair : attachments) {
            pair.second->destroy(device.device());
        }
        attachments.clear();
        
        for (auto& pair : samplers) {
            vkDestroySampler(device.device(), pair.second, nullptr);
        }
        samplers.clear();
    }

    void TextureManager::cleanAttachments() {
        deleteAttachment("gPosDepth");
        deleteAttachment("gNormals");
        deleteAttachment("gAlbedo");
        deleteAttachment("gDepth");
        deleteAttachment("ssaoColor");
        deleteAttachment("ssaoBlurColor");
        deleteAttachment("deferredShading");
        
        auto it = samplers.find("colorSampler");
        if (it != samplers.end()) {
            vkDestroySampler(device.device(), it->second, nullptr);
            samplers.erase(it);
        }
    }

    void TextureManager::deleteAttachment(const std::string& name) {
        auto it = attachments.find(name);
        if (it != attachments.end()) {
            it->second->destroy(device.device());
            attachments.erase(it);
        } else {
            std::cout << "Attachment not found: " << name << "\n";
        }
    }


    void TextureManager::createTexture2D(
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

        textures[name] = std::move(texture);
    }

    std::shared_ptr<Texture> TextureManager::getTexture(const std::string& name) const {
        auto it = textures.find(name);
        if (it == textures.end()) {
            throw std::runtime_error("Texture " + name + " not found!");
        }
        return it->second;
    }

    std::shared_ptr<Texture> TextureManager::getAttachment(const std::string& name) const {
        auto it = attachments.find(name);
        if (it == attachments.end()) {
            throw std::runtime_error("Attachment " + name + " not found!");
        }

        return std::make_shared<Texture>(it->second->texture);
    }

    VkSampler TextureManager::getSampler(const std::string &name) const {
        auto it = samplers.find(name);
        if (it == samplers.end()) {
            throw std::runtime_error("Sampler " + name + " not found!");
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
        imageInfo.usage         = usage | VK_IMAGE_USAGE_SAMPLED_BIT;
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

        std::shared_ptr<FrameBufferAttachment> attachment = std::make_shared<FrameBufferAttachment>();
        attachment->texture.image = image;
        attachment->texture.memory = imageMemory;
        attachment->texture.view = imageView;
        attachment->texture.format = format;

        attachments[name] = std::move(attachment);
    }

    void TextureManager::createSampler(const std::string &name) {
        VkSampler sampler;
        
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
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
        
        samplers[name] = sampler;
    }


    VkSampler TextureManager::createSampler() {
        VkSampler sampler;
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
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

    void TextureManager::transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
        
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        VkPipelineStageFlags sourceStage;
        VkPipelineStageFlags destinationStage;

        if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
            barrier.srcAccessMask = 0;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

            sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
            destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
        } else {
            // Handle other cases as necessary
            throw std::invalid_argument("unsupported layout transition!");
        }

        vkCmdPipelineBarrier(
            commandBuffer,
            sourceStage, destinationStage,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );
    }

    void TextureManager::createTexture2DFromBuffer(
        const std::string& name,
        void* buffer,
        VkDeviceSize bufferSize,
        VkFormat format,
        uint32_t width,
        uint32_t height,
        VkFilter filter,
        VkImageUsageFlags imageUsageFlags,
        VkImageLayout imageLayout) {

        if (textures.find(name) != textures.end()) return;

        assert(buffer);

        auto texture = std::make_shared<Texture>();
        texture->format = format;

        // Raw image data staging
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingMemory;

        VkBufferCreateInfo bufferCreateInfo = {};
        bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferCreateInfo.size = bufferSize;
        bufferCreateInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
        bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        if (vkCreateBuffer(device.device(), &bufferCreateInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
            throw std::runtime_error("failed to create buffer in createTexture2DFromBuffer!");
        }

        VkMemoryRequirements memReqs;
        vkGetBufferMemoryRequirements(device.device(), stagingBuffer, &memReqs);

        VkMemoryAllocateInfo memAllocInfo = {};
        memAllocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        memAllocInfo.allocationSize = memReqs.size;
        memAllocInfo.memoryTypeIndex = device.findMemoryType(memReqs.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        if (vkAllocateMemory(device.device(), &memAllocInfo, nullptr, &stagingMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate buffer memory in createTexture2DFromBuffer!");
        }

        vkBindBufferMemory(device.device(), stagingBuffer, stagingMemory, 0);

        // Copy texture data into staging buffer
        void* data;
        vkMapMemory(device.device(), stagingMemory, 0, bufferSize, 0, &data);
        memcpy(data, buffer, static_cast<size_t>(bufferSize));
        vkUnmapMemory(device.device(), stagingMemory);

        // Create the image
        createImage(width, height, format, imageUsageFlags | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, 1, VK_SAMPLE_COUNT_1_BIT, VK_IMAGE_TILING_OPTIMAL, texture->image, texture->memory);

        // Use a command buffer for copy and layout transitions
        VkCommandBuffer commandBuffer = device.beginSingleTimeCommands();

        // Transition the image to be a valid copy destination
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = texture->image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        // Copy buffer to image
        VkBufferImageCopy bufferCopyRegion = {};
        bufferCopyRegion.bufferOffset = 0;
        bufferCopyRegion.bufferRowLength = 0;
        bufferCopyRegion.bufferImageHeight = 0;
        bufferCopyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        bufferCopyRegion.imageSubresource.mipLevel = 0;
        bufferCopyRegion.imageSubresource.baseArrayLayer = 0;
        bufferCopyRegion.imageSubresource.layerCount = 1;
        bufferCopyRegion.imageOffset = { 0, 0, 0 };
        bufferCopyRegion.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(commandBuffer, stagingBuffer, texture->image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &bufferCopyRegion);

        // Transition the image to be shader readable
        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = imageLayout;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0,
            0, nullptr,
            0, nullptr,
            1, &barrier
        );

        device.endSingleTimeCommands(commandBuffer);

        // Clean up staging resources
        vkDestroyBuffer(device.device(), stagingBuffer, nullptr);
        vkFreeMemory(device.device(), stagingMemory, nullptr);

        // Create image view
        texture->view = createImageView(texture->image, format, VK_IMAGE_ASPECT_COLOR_BIT, 1);

        // Create sampler
        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = filter;
        samplerCreateInfo.minFilter = filter;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerCreateInfo.anisotropyEnable = VK_TRUE;
        samplerCreateInfo.maxAnisotropy = 16;
        samplerCreateInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerCreateInfo.unnormalizedCoordinates = VK_FALSE;
        samplerCreateInfo.compareEnable = VK_FALSE;
        samplerCreateInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerCreateInfo.mipLodBias = 0.0f;
        samplerCreateInfo.minLod = 0.0f;
        samplerCreateInfo.maxLod = 1.0f;

        if (vkCreateSampler(device.device(), &samplerCreateInfo, nullptr, &texture->sampler) != VK_SUCCESS) {
            throw std::runtime_error("failed to create texture sampler!");
        }

        textures[name] = std::move(texture);
    }
}
