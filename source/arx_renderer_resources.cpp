#include <stdio.h>
#include "arx_renderer.h"

// std
#include <stdexcept>
#include <array>
#include <cassert>

namespace arx {

    struct PushConstantData {
        glm::mat4 modelMatrix{1.f};
        glm::mat4 normalMatrix{1.f};
    };

    namespace {
        enum class PassName {
            GPass,
            SSAO,
            // more passes...
            Max
        };
    
        std::array<std::vector<std::shared_ptr<ArxDescriptorSetLayout>>, static_cast<size_t>(PassName::Max)> descriptorLayouts;
        std::array<std::shared_ptr<ArxDescriptorPool>, static_cast<uint8_t>(PassName::Max)>         descriptorPools;
        std::array<std::vector<VkDescriptorSet>, static_cast<uint8_t>(PassName::Max)>               descriptorSets;
        std::array<VkPipelineLayout, static_cast<uint8_t>(PassName::Max)>                           pipelineLayouts;
        std::array<std::shared_ptr<ArxPipeline>, static_cast<uint8_t>(PassName::Max)>               pipelines;
    
        std::unordered_map<uint8_t, std::vector<std::shared_ptr<ArxBuffer>>>                        passBuffers;
    }

    void ArxRenderer::createDescriptorSetLayouts() {
        // G-Buffer
        descriptorLayouts[static_cast<uint8_t>(PassName::GPass)].push_back(ArxDescriptorSetLayout::Builder(arxDevice)
                                            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                                            .build());
        
        // TODO: continue with the ssao pass
    }

    void ArxRenderer::createPipelineLayouts() {
        // G-Buffer
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags    = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset        = 0;
        pushConstantRange.size          = sizeof(PushConstantData);
        
        std::vector<VkDescriptorSetLayout> vkLayouts;
        for (const auto& layout : descriptorLayouts[static_cast<size_t>(PassName::GPass)]) {
            vkLayouts.push_back(layout->getDescriptorSetLayout());
        }
    
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(vkLayouts.size());
        pipelineLayoutInfo.pSetLayouts              = vkLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount   = 1;
        pipelineLayoutInfo.pPushConstantRanges      = &pushConstantRange;
        
        if (vkCreatePipelineLayout(arxDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayouts[static_cast<uint8_t>(PassName::GPass)]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create gpass pipeline layout!");
        }
    }

    void ArxRenderer::createPipelines() {
        // G-Buffer
        assert(pipelineLayouts[static_cast<uint8_t>(PassName::GPass)] != nullptr && "Cannot create pipeline before pipeline layout");
        
        PipelineConfigInfo configInfo{};
        VkPipelineColorBlendAttachmentState attachment1 = ArxPipeline::createDefaultColorBlendAttachment();
        VkPipelineColorBlendAttachmentState attachment2 = ArxPipeline::createDefaultColorBlendAttachment();
        VkPipelineColorBlendAttachmentState attachment3 = ArxPipeline::createDefaultColorBlendAttachment();
        configInfo.colorBlendAttachments = {attachment1, attachment2, attachment3};
        
        ArxPipeline::defaultPipelineConfigInfo(configInfo);
        configInfo.renderPass       = rpManager.getRenderPass("GBuffer");
        configInfo.pipelineLayout   = pipelineLayouts[static_cast<uint8_t>(PassName::GPass)];
        
        pipelines[static_cast<uint8_t>(PassName::GPass)] = std::make_shared<ArxPipeline>(arxDevice,
                                                                                        "shaders/gbuffer_vert.spv",
                                                                                        "shaders/gbuffer_frag.spv",
                                                                                         configInfo);
    }

    void ArxRenderer::createUniformBuffers() {
        // G-Buffer
        // UBO
        descriptorPools[static_cast<uint8_t>(PassName::GPass)] = ArxDescriptorPool::Builder(arxDevice)
                                                                .setMaxSets(1)
                                                                .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f)
                                                                .build();
        
        passBuffers[static_cast<uint8_t>(PassName::GPass)].push_back(std::make_shared<ArxBuffer>(
                                                                      arxDevice,
                                                                      sizeof(GlobalUbo),
                                                                      1,
                                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
        
        passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->map();
        passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->writeToBuffer(&ubo);
        
        descriptorSets[static_cast<uint8_t>(PassName::GPass)].resize(1);
        
        for (size_t i = 0; i < passBuffers[static_cast<uint8_t>(PassName::GPass)].size(); ++i) {
            auto bufferInfo = passBuffers[static_cast<uint8_t>(PassName::GPass)][i]->descriptorInfo();
            ArxDescriptorWriter(*descriptorLayouts[static_cast<uint8_t>(PassName::GPass)][0],
                                *descriptorPools[static_cast<uint8_t>(PassName::GPass)])
                                .writeBuffer(0, &bufferInfo)
                                .build(descriptorSets[static_cast<uint8_t>(PassName::GPass)][0]);
        }
    }

    void ArxRenderer::cleanupResources() {
        for (VkPipelineLayout layout : pipelineLayouts) {
            if (layout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(arxDevice.device(), layout, nullptr);
            }
        }
    }

    void ArxRenderer::createRenderPasses() {
        VkFormat depthFormat = arxSwapChain->findDepthFormat();
        
        // attachments
        textureManager.createAttachment("gPosDepth", arxSwapChain->width(), arxSwapChain->height(),
                                        VK_FORMAT_R32G32B32A32_SFLOAT,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        textureManager.createAttachment("gNormals", arxSwapChain->width(), arxSwapChain->height(),
                                        VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        textureManager.createAttachment("gAlbedo", arxSwapChain->width(), arxSwapChain->height(),
                                        VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        textureManager.createAttachment("gDepth", arxSwapChain->width(), arxSwapChain->height(),
                                        depthFormat,
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        
        textureManager.createAttachment("ssaoColor", arxSwapChain->width(), arxSwapChain->height(),
                                        VK_FORMAT_R8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        textureManager.createAttachment("ssaoBlurColor", arxSwapChain->width(), arxSwapChain->height(),
                                        VK_FORMAT_R8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        // G-Pass
        {
            std::array<VkAttachmentDescription, 4> attachmentDescs = {};
            for (uint32_t i = 0; i < static_cast<uint32_t>(attachmentDescs.size()); i++) {
                attachmentDescs[i].samples = VK_SAMPLE_COUNT_1_BIT;
                attachmentDescs[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachmentDescs[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
                attachmentDescs[i].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachmentDescs[i].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachmentDescs[i].finalLayout = (i == 3) ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                attachmentDescs[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            }
            
            // Formats
            attachmentDescs[0].format = textureManager.getAttachment("gPosDepth")->format;
            attachmentDescs[1].format = textureManager.getAttachment("gNormals")->format;
            attachmentDescs[2].format = textureManager.getAttachment("gAlbedo")->format;
            attachmentDescs[3].format = depthFormat;
            
            
            std::vector<VkAttachmentReference> colorReferences;
            colorReferences.push_back({ 0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
            colorReferences.push_back({ 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
            colorReferences.push_back({ 2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL });
        
            VkAttachmentReference depthReference = {};
            depthReference.attachment = 3;
            depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            
            VkSubpassDescription subpass = {};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.pColorAttachments = colorReferences.data();
            subpass.colorAttachmentCount = static_cast<uint32_t>(colorReferences.size());
            subpass.pDepthStencilAttachment = &depthReference;
            
            // Use subpass dependencies for attachment layout transitions
            std::array<VkSubpassDependency, 2> dependencies;
            
            dependencies[0].srcSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[0].dstSubpass = 0;
            dependencies[0].srcStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[0].srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            
            dependencies[1].srcSubpass = 0;
            dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;
            
            
            VkRenderPassCreateInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.pAttachments = attachmentDescs.data();
            renderPassInfo.attachmentCount = static_cast<uint32_t>(attachmentDescs.size());
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
            renderPassInfo.pDependencies = dependencies.data();
            
            rpManager.createRenderPass("GBuffer", renderPassInfo);
            
            std::array<VkImageView, 4> attachments;
            attachments[0] = textureManager.getAttachment("gPosDepth")->view;
            attachments[1] = textureManager.getAttachment("gNormals")->view;
            attachments[2] = textureManager.getAttachment("gAlbedo")->view;
            attachments[3] = textureManager.getAttachment("gDepth")->view;
            
            rpManager.createFramebuffer("GBuffer", attachments, arxSwapChain->width(), arxSwapChain->height());
        }
        
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
            renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
            renderPassInfo.pDependencies = dependencies.data();
            
            rpManager.createRenderPass("SSAO", renderPassInfo);
            
            // attachments
            std::array<VkImageView, 1> attachments;
            attachments[0] = textureManager.getAttachment("ssaoColor")->view;
                    
            rpManager.createFramebuffer("SSAO", attachments, arxSwapChain->width(), arxSwapChain->height());
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
            renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
            renderPassInfo.pDependencies = dependencies.data();
            
            rpManager.createRenderPass("SSAOBlur", renderPassInfo);
    
            std::array<VkImageView, 1> attachments;
            attachments[0] = textureManager.getAttachment("ssaoBlurColor")->view;
            
            rpManager.createFramebuffer("SSAOBlur", attachments, arxSwapChain->width(), arxSwapChain->height());
        }
    }

    void ArxRenderer::Pass_GBuffer(FrameInfo &frameInfo, std::vector<uint32_t> &visibleChunksIndices) {
        
        beginRenderPass(frameInfo, "GBuffer");
        
        pipelines[static_cast<uint8_t>(PassName::GPass)]->bind(frameInfo.commandBuffer);
    
        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayouts[static_cast<uint8_t>(PassName::GPass)],
                                0,
                                1,
                                &descriptorSets[static_cast<uint8_t>(PassName::GPass)][0],
                                0,
                                nullptr);
        
        for (auto i : visibleChunksIndices) {
            if (frameInfo.voxel[i].model == nullptr) continue;
            PushConstantData push{};
            frameInfo.voxel[i].transform.scale = glm::vec3(VOXEL_SIZE/2);
            push.modelMatrix    = frameInfo.voxel[i].transform.mat4();
            push.normalMatrix   = frameInfo.voxel[i].transform.normalMatrix();


            vkCmdPushConstants(frameInfo.commandBuffer,
                               pipelineLayouts[static_cast<uint8_t>(PassName::GPass)],
                               VK_SHADER_STAGE_VERTEX_BIT |
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(PushConstantData),
                               &push);
            
            frameInfo.voxel[i].model->bind(frameInfo.commandBuffer);
            frameInfo.voxel[i].model->draw(frameInfo.commandBuffer);
        }
        
        vkCmdEndRenderPass(frameInfo.commandBuffer);
    }

    void ArxRenderer::updateMisc(const GlobalUbo &rhs) {
        ubo.projection  = rhs.projection;
        ubo.view        = rhs.view;
        ubo.inverseView = rhs.inverseView;
        ubo.zNear       = rhs.zNear;
        ubo.zFar        = rhs.zFar;
        
        passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->writeToBuffer(&ubo);
        passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->flush();
    }

    void ArxRenderer::init_Passes() {
        createRenderPasses();
        createDescriptorSetLayouts();
        createPipelineLayouts();
        createPipelines();
        createUniformBuffers();
    }
}
