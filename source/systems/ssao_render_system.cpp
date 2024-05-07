#include "ssao_render_system.hpp"


namespace arx {
    SSAOSystem::SSAOSystem(ArxDevice &device, RenderPassManager& rp, TextureManager& textures, VkExtent2D extent)
    : device(device), rpManager(rp), textureManager(textures), extent(extent) {
        createDescriptorSetLayout();
        createSSAOPipelineLayout();
        createSSAOPipeline();
    }

    SSAOSystem::~SSAOSystem() {
        vkDestroyPipelineLayout(device.device(), pipelineLayouts.ssao, nullptr);
        vkDestroyPipelineLayout(device.device(), pipelineLayouts.ssaoBlur, nullptr);
    }

    void SSAOSystem::createFramebuffers() {
        
        textureManager.createAttachment("ssaoColor", extent.width, extent.height,
                                        VK_FORMAT_R8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        
        textureManager.createAttachment("ssaoBlurColor", extent.width, extent.height,
                                        VK_FORMAT_R8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        // RenderPasses
        
        // SSAO
        {
            VkAttachmentDescription attachmentDescription{};
            attachmentDescription.format = VK_FORMAT_R8_UNORM;
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
            
            rpManager.createRenderPass("SSAO", renderPassInfo);

            std::array<VkImageView, 1> attachments;
            attachments[0] = textureManager.getTexture("ssaoColor")->view;
                    
            rpManager.createFramebuffer("SSAO", attachments, extent.width, extent.height);

        }

        // SSAO Blur
        {
            VkAttachmentDescription attachmentDescription{};
            attachmentDescription.format = VK_FORMAT_R8_UNORM;
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
            
            rpManager.createRenderPass("SSAOBlur", renderPassInfo);
    
            std::array<VkImageView, 1> attachments;
            attachments[0] = textureManager.getTexture("ssaoBlurColor")->view;
            
            rpManager.createFramebuffer("SSAOBlur", attachments, extent.width, extent.height);
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
        renderPassInfo.renderPass   = blurPass ? rpManager.getRenderPass("SSAO") : rpManager.getRenderPass("SSAOBlur");
        renderPassInfo.framebuffer  = blurPass ? rpManager.getFrameBuffer("SSAO") : rpManager.getFrameBuffer("SSAOBlur");
        renderPassInfo.renderArea.extent.width = extent.width;
        renderPassInfo.renderArea.extent.height = extent.height;
        
        std::array<VkClearValue, 2> clearValues{};
        clearValues[0].color            = {0.0f, 0.0f, 0.0f, 1.0f};
        clearValues[1].depthStencil     = {1.0f, 0};
        renderPassInfo.clearValueCount  = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues     = clearValues.data();
        
        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        
        VkViewport viewport{};
        viewport.x  = 0.0f;
        viewport.y  = 0.0f;
        viewport.width  = static_cast<float>(extent.width);
        viewport.height = static_cast<float>(extent.height);
        viewport.minDepth   = 0.0f;
        viewport.maxDepth   = 1.0f;
        VkRect2D scissor{{0, 0}, {static_cast<uint32_t>(extent.width), static_cast<uint32_t>(extent.height)}};
        vkCmdSetViewport(commandBuffer, 0, 1, &viewport);
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    }

    void SSAOSystem::endRenderPass(VkCommandBuffer commandBuffer) {
        vkCmdEndRenderPass(commandBuffer);
    }
}
