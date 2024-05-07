#include "ssao_render_system.hpp"


namespace arx {
    SSAOSystem::SSAOSystem(ArxDevice &device, RenderPassManager& renderPassManager, VkExtent2D extent)
    : device(device), ssao(renderPassManager.getRenderPass("SSAO")), extent(extent) {
        createDescriptorSetLayout();
        createSSAOPipelineLayout();
        createSSAOPipeline();
    }

    SSAOSystem::~SSAOSystem() {
        vkDestroyPipelineLayout(device.device(), pipelineLayouts.ssao, nullptr);
        vkDestroyPipelineLayout(device.device(), pipelineLayouts.ssaoBlur, nullptr);
        
        // Attachments
        ssao.color.destroy(device.device());
        ssaoBlur.color.destroy(device.device());
        
        // FrameBuffers
        ssao.destroy(device.device());
        ssaoBlur.destroy(device.device());
    }

    void SSAOSystem::createFramebuffers() {
        ssao.setSize(extent.width, extent.height);
        ssaoBlur.setSize(extent.width, extent.height);
        
        createAttachment(extent.width, extent.height, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &ssao.color);
        createAttachment(extent.width, extent.height, VK_FORMAT_R8_UNORM, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, &ssaoBlur.color);
        
        // RenderPasses
        
        // SSAO
        {
            VkAttachmentDescription attachmentDescription{};
            attachmentDescription.format = ssao.color.format;
            attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
            attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.pColorAttachments = &colorReference;
            subpass.colorAttachmentCount = 1;

            std::array<VkSubpassDependency, 2> dependencies;

            dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[0].dstSubpass = 0;
            dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            dependencies[1].srcSubpass = 0;
            dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            VkRenderPassCreateInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.pAttachments = &attachmentDescription;
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 2;
            renderPassInfo.pDependencies = dependencies.data();
            if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &ssao.renderPass) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create ssao render pass!");
            }

            VkFramebufferCreateInfo fbufCreateInfo{};
            fbufCreateInfo.renderPass = ssao.renderPass;
            fbufCreateInfo.pAttachments = &ssao.color.view;
            fbufCreateInfo.attachmentCount = 1;
            fbufCreateInfo.width = ssao.width;
            fbufCreateInfo.height = ssao.height;
            fbufCreateInfo.layers = 1;
            if (vkCreateFramebuffer(device.device(), &fbufCreateInfo, nullptr, &ssao.frameBuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create ssao framebuffer!");
            }
        }

        // SSAO Blur
        {
            VkAttachmentDescription attachmentDescription{};
            attachmentDescription.format = ssaoBlur.color.format;
            attachmentDescription.samples = VK_SAMPLE_COUNT_1_BIT;
            attachmentDescription.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            attachmentDescription.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            attachmentDescription.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            attachmentDescription.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            attachmentDescription.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            attachmentDescription.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

            VkAttachmentReference colorReference = { 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL };

            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.pColorAttachments = &colorReference;
            subpass.colorAttachmentCount = 1;

            std::array<VkSubpassDependency, 2> dependencies;

            dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[0].dstSubpass = 0;
            dependencies[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            dependencies[1].srcSubpass = 0;
            dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
            dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            VkRenderPassCreateInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.pAttachments = &attachmentDescription;
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 2;
            renderPassInfo.pDependencies = dependencies.data();
            if (vkCreateRenderPass(device.device(), &renderPassInfo, nullptr, &ssaoBlur.renderPass) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create ssaoBlur renderpass!");
            }

            VkFramebufferCreateInfo fbufCreateInfo{};
            fbufCreateInfo.renderPass = ssaoBlur.renderPass;
            fbufCreateInfo.pAttachments = &ssaoBlur.color.view;
            fbufCreateInfo.attachmentCount = 1;
            fbufCreateInfo.width = ssaoBlur.width;
            fbufCreateInfo.height = ssaoBlur.height;
            fbufCreateInfo.layers = 1;
            if (vkCreateFramebuffer(device.device(), &fbufCreateInfo, nullptr, &ssaoBlur.frameBuffer) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create ssaoBlur framebuffer!");
            }
        }
        
        // Shared sampler used for all color attachments
        VkSamplerCreateInfo sampler{};
        sampler.magFilter = VK_FILTER_NEAREST;
        sampler.minFilter = VK_FILTER_NEAREST;
        sampler.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        sampler.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        sampler.mipLodBias = 0.0f;
        sampler.maxAnisotropy = 1.0f;
        sampler.minLod = 0.0f;
        sampler.maxLod = 1.0f;
        sampler.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        if (vkCreateSampler(device.device(), &sampler, nullptr, &colorSampler) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create color sampler!");
        }
    }

    void SSAOSystem::createDescriptorSetLayout() {
        
    }

    void SSAOSystem::createSSAOPipelineLayout() {
        
    }

    void SSAOSystem::createSSAOPipeline() {
        
    }

    void SSAOSystem::createSSAOBlurPipelineLayout() {
        
    }

    void SSAOSystem::createSSAOBLurPipeline() {
        
    }

    void SSAOSystem::renderSSAO(FrameInfo &frameInfo, bool blurPass) {
        if (!blurPass) {
            pipelines.ssao->bind(frameInfo.commandBuffer);
        } else {
            pipelines.ssaoBlur->bind(frameInfo.commandBuffer);
        }
        
        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                            VK_PIPELINE_BIND_POINT_GRAPHICS,
                            blurPass ? pipelineLayouts.ssaoBlur : pipelineLayouts.ssao, 0, 1,
                            blurPass ? &descriptorSets.ssaoBlur : &descriptorSets.ssao, 0, nullptr);
        
        // TODO: implement drawing
        
    }

    void SSAOSystem::beginRenderPass(VkCommandBuffer commandBuffer, bool blurPass) {
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType        = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass   = blurPass ? ssaoBlur.renderPass : ssao.renderPass;
        renderPassInfo.framebuffer  = blurPass ? ssaoBlur.frameBuffer : ssao.frameBuffer;
        renderPassInfo.renderArea.extent.width = blurPass ? ssaoBlur.width : ssao.width;
        renderPassInfo.renderArea.extent.height = blurPass ? ssaoBlur.height : ssao.height;
        
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color            = {0.0f, 0.0f, 0.0f, 1.0f};
        clearValues[1].depthStencil     = {1.0f, 0};
        renderPassInfo.clearValueCount  = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues     = clearValues.data();
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        VkViewport viewport{};
        viewport.x  = 0.0f;
        viewport.y  = 0.0f;
        viewport.width  = static_cast<float>(blurPass ? ssaoBlur.width : ssao.width);
        viewport.height = static_cast<float>(blurPass ? ssaoBlur.height : ssao.height);
        viewport.minDepth   = 0.0f;
        viewport.maxDepth   = 1.0f;
        VkRect2D scissor{{0, 0}, {static_cast<uint32_t>(blurPass ? ssaoBlur.width : ssao.width), static_cast<uint32_t>(blurPass ? ssaoBlur.height : ssao.height)}};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    void SSAOSystem::endRenderPass(VkCommandBuffer commandBuffer) {
        vkCmdEndRenderPass(commandBuffer);
    }

    void SSAOSystem::createAttachment(uint32_t width, uint32_t height, VkFormat format, VkImageUsageFlags usage, FrameBufferAttachment *attachment) {
        VkImageAspectFlags aspectMask = 0;

        attachment->format = format;

        if (usage & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT)
        {
            aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        }
        if (usage & VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT)
        {
            aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            if (format >= VK_FORMAT_D16_UNORM_S8_UINT)
                aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
        }

        assert(aspectMask > 0);
        
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width  = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = format;
        imageInfo.tiling        = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage         = usage;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.mipLevels     = 1;
        imageInfo.samples       = VK_SAMPLE_COUNT_1_BIT;

        if (vkCreateImage(device.device(), &imageInfo, nullptr, &attachment->image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device.device(), attachment->image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = device.findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device.device(), &allocInfo, nullptr, &attachment->mem) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device.device(), attachment->image, attachment->mem, 0);
        
        VkImageViewCreateInfo imageView{};
        imageView.viewType = VK_IMAGE_VIEW_TYPE_2D;
        imageView.format = format;
        imageView.subresourceRange = {};
        imageView.subresourceRange.aspectMask = aspectMask;
        imageView.subresourceRange.baseMipLevel = 0;
        imageView.subresourceRange.levelCount = 1;
        imageView.subresourceRange.baseArrayLayer = 0;
        imageView.subresourceRange.layerCount = 1;
        imageView.image = attachment->image;
        
        if (vkCreateImageView(device.device(), &imageView, nullptr, &attachment->view) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create attachment image view!");
        }
    }
}
