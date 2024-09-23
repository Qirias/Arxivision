#include "../source/engine_pch.hpp"

#include "../source/arx_renderer.h"

#include "../libs/imgui/imgui.h"
#include "../libs/imgui/backends/imgui_impl_glfw.h"
#include "../libs/imgui/backends/imgui_impl_vulkan.h"
#include "geometry/LTC.h"

#include "../source/systems/clustered_shading_system.hpp"
#include "../source/geometry/blockMaterials.hpp"

namespace arx {

    struct PushConstantData {
        glm::mat4 modelMatrix{1.f};
        glm::mat4 normalMatrix{1.f};
    };

    namespace {
        enum class PassName {
            // Offscreen
            GPass,
            SSAO,
            SSAOBLUR,
            DEFERRED,
            COMPOSITION,
            // Present final view to the viewport
            IMGUI,
            Max
        };
    
        std::array<std::vector<std::shared_ptr<ArxDescriptorSetLayout>>, static_cast<size_t>(PassName::Max)>    descriptorLayouts;
        std::array<std::shared_ptr<ArxDescriptorPool>, static_cast<uint8_t>(PassName::Max)>                     descriptorPools;
        std::array<std::vector<VkDescriptorSet>, static_cast<uint8_t>(PassName::Max)>                           descriptorSets;
        std::array<VkPipelineLayout, static_cast<uint8_t>(PassName::Max)>                                       pipelineLayouts;
        std::array<std::shared_ptr<ArxPipeline>, static_cast<uint8_t>(PassName::Max)>                           pipelines;
    
        std::unordered_map<uint8_t, std::vector<std::shared_ptr<ArxBuffer>>>                                    passBuffers;
    }

    void ArxRenderer::createDescriptorSetLayouts() {
        // G-Buffer
        descriptorLayouts[static_cast<uint8_t>(PassName::GPass)].push_back(ArxDescriptorSetLayout::Builder(arxDevice)
                                        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS) // InstanceBuffer
                                        .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT) // UBO
                                        .build());
        
        // SSAO
        descriptorLayouts[static_cast<uint8_t>(PassName::SSAO)].push_back(ArxDescriptorSetLayout::Builder(arxDevice)
                                        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // samplerPosDepth
                                        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // samplerNormal
                                        .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // ssaoNoise
                                        .addBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // UBOSSAOKernel
                                        .addBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // UBO
                                        .build());
        
        // SSAOBlur
        descriptorLayouts[static_cast<uint8_t>(PassName::SSAOBLUR)].push_back(ArxDescriptorSetLayout::Builder(arxDevice)
                                        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // samplerSSAO
                                        .build());
        
        // Deferred
        descriptorLayouts[static_cast<uint8_t>(PassName::DEFERRED)].push_back(ArxDescriptorSetLayout::Builder(arxDevice)
                                        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // samplerPosDepth
                                        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // samplerNormal
                                        .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // samplerAlbedo
                                        .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // LTC1
                                        .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // LTC2
                                        .addBinding(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // UBO
                                        .addBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // PointLightsBuffer
                                        .addBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // Frustum Clusters
                                        .addBinding(8, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // lightCount
                                        .addBinding(9, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // Frustum params
                                        .addBinding(10, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // Editor Params
                                        .build());
        
        // Composition
        descriptorLayouts[static_cast<uint8_t>(PassName::COMPOSITION)].push_back(ArxDescriptorSetLayout::Builder(arxDevice)
                                        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // samplerPosDepth
                                        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // samplerNormal
                                        .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // samplerAlbedo
                                        .addBinding(3, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // samplerSSAO
                                        .addBinding(4, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // samplerSSAOBlur
                                        .addBinding(5, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT) // samplerDeferred
                                        .addBinding(6, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // UBO
                                        .addBinding(7, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                        .build());

        // ImGUI
        descriptorLayouts[static_cast<uint8_t>(PassName::IMGUI)].push_back(ArxDescriptorSetLayout::Builder(arxDevice)
                                        .addBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT)
                                        .build());
    }

    void ArxRenderer::createPipelineLayouts() {
        // ====================================================================================
        //                                      G-Buffer
        // ====================================================================================

        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags    = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset        = 0;
        pushConstantRange.size          = sizeof(PushConstantData);
        
        std::vector<VkDescriptorSetLayout> gPassLayouts;
        for (const auto& layout : descriptorLayouts[static_cast<size_t>(PassName::GPass)]) {
            gPassLayouts.push_back(layout->getDescriptorSetLayout());
        }
    
        VkPipelineLayoutCreateInfo gPassPipelineLayoutInfo{};
        gPassPipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        gPassPipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(gPassLayouts.size());
        gPassPipelineLayoutInfo.pSetLayouts              = gPassLayouts.data();
        gPassPipelineLayoutInfo.pushConstantRangeCount   = 1;
        gPassPipelineLayoutInfo.pPushConstantRanges      = &pushConstantRange;
        
        if (vkCreatePipelineLayout(arxDevice.device(), &gPassPipelineLayoutInfo, nullptr, &pipelineLayouts[static_cast<uint8_t>(PassName::GPass)]) != VK_SUCCESS) {
            ARX_LOG_ERROR("failed to create gpass pipeline layout!");
        }
        
        // ====================================================================================
        //                                      SSAO
        // ====================================================================================
        
        std::vector<VkDescriptorSetLayout> ssaoLayouts;
        for (const auto& layout : descriptorLayouts[static_cast<size_t>(PassName::SSAO)]) {
            ssaoLayouts.push_back(layout->getDescriptorSetLayout());
        }
    
        VkPipelineLayoutCreateInfo ssaoPipelineLayoutInfo{};
        ssaoPipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ssaoPipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(ssaoLayouts.size());
        ssaoPipelineLayoutInfo.pSetLayouts              = ssaoLayouts.data();
        
        if (vkCreatePipelineLayout(arxDevice.device(), &ssaoPipelineLayoutInfo, nullptr, &pipelineLayouts[static_cast<uint8_t>(PassName::SSAO)]) != VK_SUCCESS) {
            ARX_LOG_ERROR("failed to create ssao pipeline layout!");
        }
        
        
        // ====================================================================================
        //                                    SSAOBLUR
        // ====================================================================================
        
        std::vector<VkDescriptorSetLayout> ssaoBlurLayouts;
        for (const auto& layout : descriptorLayouts[static_cast<size_t>(PassName::SSAOBLUR)]) {
            ssaoBlurLayouts.push_back(layout->getDescriptorSetLayout());
        }
    
        VkPipelineLayoutCreateInfo ssaoBlurPipelineLayoutInfo{};
        ssaoBlurPipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        ssaoBlurPipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(ssaoBlurLayouts.size());
        ssaoBlurPipelineLayoutInfo.pSetLayouts              = ssaoBlurLayouts.data();
        
        if (vkCreatePipelineLayout(arxDevice.device(), &ssaoBlurPipelineLayoutInfo, nullptr, &pipelineLayouts[static_cast<uint8_t>(PassName::SSAOBLUR)]) != VK_SUCCESS) {
            ARX_LOG_ERROR("failed to create ssaoblur pipeline layout!");
        }
        
        // ====================================================================================
        //                                    DEFERRED
        // ====================================================================================
        
        std::vector<VkDescriptorSetLayout> deferredLayouts;
        for (const auto& layout : descriptorLayouts[static_cast<size_t>(PassName::DEFERRED)]) {
            deferredLayouts.push_back(layout->getDescriptorSetLayout());
        }
    
        VkPipelineLayoutCreateInfo deferredPipelineLayoutInfo{};
        deferredPipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        deferredPipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(deferredLayouts.size());
        deferredPipelineLayoutInfo.pSetLayouts              = deferredLayouts.data();
        
        if (vkCreatePipelineLayout(arxDevice.device(), &deferredPipelineLayoutInfo, nullptr, &pipelineLayouts[static_cast<uint8_t>(PassName::DEFERRED)]) != VK_SUCCESS) {
            ARX_LOG_ERROR("failed to create deferred pipeline layout!");
        }
        
        // ====================================================================================
        //                                   COMPOSITION
        // ====================================================================================
        
        std::vector<VkDescriptorSetLayout> compositionLayouts;
        for (const auto& layout : descriptorLayouts[static_cast<size_t>(PassName::COMPOSITION)]) {
            compositionLayouts.push_back(layout->getDescriptorSetLayout());
        }
    
        VkPipelineLayoutCreateInfo compositionPipelineLayoutInfo{};
        compositionPipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        compositionPipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(compositionLayouts.size());
        compositionPipelineLayoutInfo.pSetLayouts              = compositionLayouts.data();
        
        if (vkCreatePipelineLayout(arxDevice.device(), &compositionPipelineLayoutInfo, nullptr, &pipelineLayouts[static_cast<uint8_t>(PassName::COMPOSITION)]) != VK_SUCCESS) {
            ARX_LOG_ERROR("failed to create composition pipeline layout!");
        }

        // ====================================================================================
        //                                      IMGUI
        // ====================================================================================
        
        std::vector<VkDescriptorSetLayout> imguiLayouts;
        for (const auto& layout : descriptorLayouts[static_cast<size_t>(PassName::IMGUI)]) {
            imguiLayouts.push_back(layout->getDescriptorSetLayout());
        }
    
        VkPipelineLayoutCreateInfo imguiPipelineLayoutInfo{};
        imguiPipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        imguiPipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(imguiLayouts.size());
        imguiPipelineLayoutInfo.pSetLayouts              = imguiLayouts.data();
        
        if (vkCreatePipelineLayout(arxDevice.device(), &imguiPipelineLayoutInfo, nullptr, &pipelineLayouts[static_cast<uint8_t>(PassName::IMGUI)]) != VK_SUCCESS) {
            ARX_LOG_ERROR("failed to create imgui pipeline layout!");
        }
        
    }

    void ArxRenderer::createPipelines() {
        // ====================================================================================
        //                                      G-Buffer
        // ====================================================================================

        assert(pipelineLayouts[static_cast<uint8_t>(PassName::GPass)] != nullptr && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo gPassConfigInfo{};
        VkPipelineColorBlendAttachmentState attachment1 = ArxPipeline::createDefaultColorBlendAttachment();
        VkPipelineColorBlendAttachmentState attachment2 = ArxPipeline::createDefaultColorBlendAttachment();
        VkPipelineColorBlendAttachmentState attachment3 = ArxPipeline::createDefaultColorBlendAttachment();
        gPassConfigInfo.colorBlendAttachments = {attachment1, attachment2, attachment3};
        
        ArxPipeline::defaultPipelineConfigInfo(gPassConfigInfo);
        gPassConfigInfo.renderPass                      = rpManager.getRenderPass("GBuffer");
        gPassConfigInfo.pipelineLayout                  = pipelineLayouts[static_cast<uint8_t>(PassName::GPass)];
        gPassConfigInfo.colorBlendInfo.attachmentCount  = 3;
        gPassConfigInfo.colorBlendInfo.pAttachments     = gPassConfigInfo.colorBlendAttachments.data();

        pipelines[static_cast<uint8_t>(PassName::GPass)] = std::make_shared<ArxPipeline>(arxDevice,
                                                                                        "shaders/gbuffer_vert.spv",
                                                                                        "shaders/gbuffer_frag.spv",
                                                                                        "",
                                                                                         gPassConfigInfo);

        // ====================================================================================
        //                                    COMPOSITION
        // ====================================================================================
        
        assert(pipelineLayouts[static_cast<uint8_t>(PassName::COMPOSITION)] != nullptr && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo compositionConfigInfo{};
        ArxPipeline::defaultPipelineConfigInfo(compositionConfigInfo);
        compositionConfigInfo.renderPass                    = rpManager.getRenderPass("Composition");
        compositionConfigInfo.pipelineLayout                = pipelineLayouts[static_cast<uint8_t>(PassName::COMPOSITION)];
        compositionConfigInfo.rasterizationInfo.cullMode    = VK_CULL_MODE_FRONT_BIT;
        compositionConfigInfo.rasterizationInfo.frontFace   = VK_FRONT_FACE_COUNTER_CLOCKWISE;
        compositionConfigInfo.useVertexInputState           = false;
        VkPipelineColorBlendAttachmentState compositionColorBlendAttachment = ArxPipeline::createDefaultColorBlendAttachment();
        compositionConfigInfo.colorBlendAttachments                 = {compositionColorBlendAttachment};
        compositionConfigInfo.colorBlendInfo.attachmentCount        = 1;
        compositionConfigInfo.colorBlendInfo.pAttachments           = compositionConfigInfo.colorBlendAttachments.data();
        compositionConfigInfo.multisampleInfo.rasterizationSamples  = arxDevice.msaaSamples; // Final image with the msaaSamples

        pipelines[static_cast<uint8_t>(PassName::COMPOSITION)] = std::make_shared<ArxPipeline>(arxDevice,
                                                                                               "shaders/fullscreen.spv",
                                                                                               "shaders/composition.spv",
                                                                                               "",
                                                                                               compositionConfigInfo);

        // ====================================================================================
        //                                      SSAO
        // ====================================================================================
        
        assert(pipelineLayouts[static_cast<uint8_t>(PassName::SSAO)] != nullptr && "Cannot create pipeline before pipeline layout");

        struct SpecializationData {
            uint32_t kernelSize = SSAO_KERNEL_SIZE;
            float radius = SSAO_RADIUS;
        } specializationData;
        
        std::array<VkSpecializationMapEntry, 2> specializationMapEntries;
        specializationMapEntries[0].constantID = 0;
        specializationMapEntries[0].offset = offsetof(SpecializationData, kernelSize);
        specializationMapEntries[0].size = sizeof(SpecializationData::kernelSize);
        specializationMapEntries[1].constantID = 1;
        specializationMapEntries[1].offset = offsetof(SpecializationData, radius);
        specializationMapEntries[1].size = sizeof(SpecializationData::radius);
        
        VkSpecializationInfo specializationInfo{};
        specializationInfo.mapEntryCount    = specializationMapEntries.size();
        specializationInfo.pMapEntries      = specializationMapEntries.data();
        specializationInfo.dataSize         = sizeof(specializationData);
        specializationInfo.pData            = &specializationData;
        
        PipelineConfigInfo ssaoConfigInfo{};
        ArxPipeline::defaultPipelineConfigInfo(ssaoConfigInfo);
        ssaoConfigInfo.renderPass                               = rpManager.getRenderPass("SSAO");
        ssaoConfigInfo.fragmentShaderStage.pSpecializationInfo  = &specializationInfo;
        ssaoConfigInfo.pipelineLayout                           = pipelineLayouts[static_cast<uint8_t>(PassName::SSAO)];
        ssaoConfigInfo.useVertexInputState                      = false;
        ssaoConfigInfo.colorBlendInfo.attachmentCount           = 1;
        VkPipelineColorBlendAttachmentState ssaoColorBlendAttachment = ArxPipeline::createDefaultColorBlendAttachment();
        ssaoConfigInfo.colorBlendAttachments                    = {ssaoColorBlendAttachment};
        ssaoConfigInfo.colorBlendInfo.pAttachments              = ssaoConfigInfo.colorBlendAttachments.data();
                
        ssaoConfigInfo.vertexShaderStage = pipelines[static_cast<uint8_t>(PassName::COMPOSITION)]->getVertexShaderStageInfo();

        pipelines[static_cast<uint8_t>(PassName::SSAO)] = std::make_shared<ArxPipeline>(arxDevice,
                                                            pipelines[static_cast<uint8_t>(PassName::COMPOSITION)]->getVertShaderModule(), // Fullscreen triangle
                                                            "shaders/ssao.spv",
                                                            ssaoConfigInfo);

        // ====================================================================================
        //                                     SSAOBLUR
        // ====================================================================================
        
        assert(pipelineLayouts[static_cast<uint8_t>(PassName::SSAOBLUR)] != nullptr && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo ssaoBlurConfigInfo{};
        ArxPipeline::defaultPipelineConfigInfo(ssaoBlurConfigInfo);
        ssaoBlurConfigInfo.renderPass                           = rpManager.getRenderPass("SSAOBlur");
        ssaoBlurConfigInfo.pipelineLayout                       = pipelineLayouts[static_cast<uint8_t>(PassName::SSAOBLUR)];
        ssaoBlurConfigInfo.colorBlendInfo.attachmentCount       = 1;
        VkPipelineColorBlendAttachmentState ssaoBlurColorBlendAttachment = ArxPipeline::createDefaultColorBlendAttachment();
        ssaoBlurConfigInfo.colorBlendAttachments                = {ssaoBlurColorBlendAttachment};
        ssaoBlurConfigInfo.colorBlendInfo.pAttachments          = ssaoBlurConfigInfo.colorBlendAttachments.data();
        ssaoBlurConfigInfo.useVertexInputState                  = false;
        
        ssaoBlurConfigInfo.vertexShaderStage = pipelines[static_cast<uint8_t>(PassName::COMPOSITION)]->getVertexShaderStageInfo();

        pipelines[static_cast<uint8_t>(PassName::SSAOBLUR)] = std::make_shared<ArxPipeline>(arxDevice,
                                                                pipelines[static_cast<uint8_t>(PassName::COMPOSITION)]->getVertShaderModule(), // Fullscreen triangle
                                                                "shaders/ssaoBlur.spv",
                                                                ssaoBlurConfigInfo);
        
        // ====================================================================================
        //                                     Deferred
        // ====================================================================================

        assert(pipelineLayouts[static_cast<uint8_t>(PassName::DEFERRED)] != nullptr && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo deferredConfigInfo{};
        ArxPipeline::defaultPipelineConfigInfo(deferredConfigInfo);
        deferredConfigInfo.renderPass                    = rpManager.getRenderPass("Deferred");
        deferredConfigInfo.pipelineLayout                = pipelineLayouts[static_cast<uint8_t>(PassName::DEFERRED)];
        deferredConfigInfo.useVertexInputState           = false;

        VkPipelineColorBlendAttachmentState deferredColorBlendAttachment = ArxPipeline::createDefaultColorBlendAttachment();
        deferredConfigInfo.colorBlendAttachments                 = {deferredColorBlendAttachment};
        deferredConfigInfo.colorBlendInfo.attachmentCount        = 1;
        deferredConfigInfo.colorBlendInfo.pAttachments           = deferredConfigInfo.colorBlendAttachments.data();

        deferredConfigInfo.vertexShaderStage = pipelines[static_cast<uint8_t>(PassName::COMPOSITION)]->getVertexShaderStageInfo();

        pipelines[static_cast<uint8_t>(PassName::DEFERRED)] = std::make_shared<ArxPipeline>(arxDevice,
                                                                pipelines[static_cast<uint8_t>(PassName::COMPOSITION)]->getVertShaderModule(), // Fullscreen triangle
                                                                "shaders/deferred.spv",
                                                                deferredConfigInfo);

         // ====================================================================================
        //                                        IMGUI
        // ====================================================================================

        assert(pipelineLayouts[static_cast<uint8_t>(PassName::IMGUI)] != nullptr && "Cannot create pipeline before pipeline layout");

        PipelineConfigInfo imguiConfigInfo{};
        ArxPipeline::defaultPipelineConfigInfo(imguiConfigInfo);

        imguiConfigInfo.renderPass                    = arxSwapChain->getRenderPass();
        imguiConfigInfo.pipelineLayout                = pipelineLayouts[static_cast<uint8_t>(PassName::IMGUI)];

        imguiConfigInfo.useVertexInputState           = false;

        VkPipelineColorBlendAttachmentState imguiColorBlendAttachment = ArxPipeline::createDefaultColorBlendAttachment();
        ArxPipeline::enableAlphaBlending(imguiConfigInfo);

        imguiConfigInfo.colorBlendAttachments                 = {imguiColorBlendAttachment};
        imguiConfigInfo.colorBlendInfo.attachmentCount        = 1;
        imguiConfigInfo.colorBlendInfo.pAttachments           = imguiConfigInfo.colorBlendAttachments.data();

        imguiConfigInfo.vertexShaderStage = pipelines[static_cast<uint8_t>(PassName::COMPOSITION)]->getVertexShaderStageInfo();

        // Load and use a fragment shader designed for ImGui rendering
        pipelines[static_cast<uint8_t>(PassName::IMGUI)] = std::make_shared<ArxPipeline>(arxDevice,
                                                                        pipelines[static_cast<uint8_t>(PassName::COMPOSITION)]->getVertShaderModule(),
                                                                        "shaders/imgui.spv",
                                                                        imguiConfigInfo);
    }


    void ArxRenderer::createUniformBuffers() {
        // ====================================================================================
        //                                      G-Buffer
        // ====================================================================================
        
        descriptorPools[static_cast<uint8_t>(PassName::GPass)] = ArxDescriptorPool::Builder(arxDevice)
                                                                       .setMaxSets(1)
                                                                       .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.f)
                                                                       .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.f)
                                                                       .build();

        passBuffers[static_cast<uint8_t>(PassName::GPass)].push_back(std::make_shared<ArxBuffer>(
                                                                     arxDevice,
                                                                     sizeof(GlobalUbo),
                                                                     1,
                                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));

        passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->map();
        passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->writeToBuffer(&ubo);
        passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->unmap();
        
        passBuffers[static_cast<uint8_t>(PassName::GPass)].push_back(BufferManager::largeInstanceBuffer);
    
        descriptorSets[static_cast<uint8_t>(PassName::GPass)].resize(1);

        auto bufferInfo = passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->descriptorInfo();
        auto instanceBufferInfo = passBuffers[static_cast<uint8_t>(PassName::GPass)][1]->descriptorInfo();

        ArxDescriptorWriter(*descriptorLayouts[static_cast<uint8_t>(PassName::GPass)][0],
                           *descriptorPools[static_cast<uint8_t>(PassName::GPass)])
                           .writeBuffer(0, &bufferInfo)
                           .writeBuffer(1, &instanceBufferInfo)
                           .build(descriptorSets[static_cast<uint8_t>(PassName::GPass)][0]);
        
        // ====================================================================================
        //                                      SSAO
        // ====================================================================================
        
        descriptorPools[static_cast<uint8_t>(PassName::SSAO)] = ArxDescriptorPool::Builder(arxDevice)
                                                                .setMaxSets(1)
                                                                .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3.0f)
                                                                .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.0f)
                                                                .build();
        // Sampler Position Depth
        VkDescriptorImageInfo samplerPosDepthInfo{};
        samplerPosDepthInfo.sampler = textureManager.getSampler("colorSampler");
        samplerPosDepthInfo.imageView = textureManager.getAttachment("gPosDepth")->view;
        samplerPosDepthInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        // Sampler Normal
        VkDescriptorImageInfo samplerNormalInfo{};
        samplerNormalInfo.sampler = textureManager.getSampler("colorSampler");
        samplerNormalInfo.imageView = textureManager.getAttachment("gNormals")->view;
        samplerNormalInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        // Sample kernel
        std::default_random_engine rndEngine((unsigned)time(nullptr));
        std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

        std::vector<glm::vec4> ssaoKernel(SSAO_KERNEL_SIZE);
        for (uint32_t i = 0; i < SSAO_KERNEL_SIZE; ++i)
        {
            glm::vec3 sample(rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine) * 2.0 - 1.0, rndDist(rndEngine));
            sample = glm::normalize(sample);
            sample *= rndDist(rndEngine);
            float scale = float(i) / float(SSAO_KERNEL_SIZE);
            scale = std::lerp(0.1f, 1.0f, scale * scale);
            ssaoKernel[i] = glm::vec4(sample * scale, 0.0f);
        }
        
        passBuffers[static_cast<uint8_t>(PassName::SSAO)].push_back(std::make_shared<ArxBuffer>(
                                                                      arxDevice,
                                                                      ssaoKernel.size() * sizeof(glm::vec4),
                                                                      1,
                                                                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                      VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
        
        passBuffers[static_cast<uint8_t>(PassName::SSAO)][0]->map();
        passBuffers[static_cast<uint8_t>(PassName::SSAO)][0]->writeToBuffer(ssaoKernel.data());
        passBuffers[static_cast<uint8_t>(PassName::SSAO)][0]->unmap();
        
        // Random noise
        std::vector<glm::vec4> noiseValues(SSAO_NOISE_DIM * SSAO_NOISE_DIM);
        for (uint32_t i = 0; i < static_cast<uint32_t>(noiseValues.size()); i++) {
            noiseValues[i] = glm::vec4(rndDist(rndEngine) * 2.0f - 1.0f, rndDist(rndEngine) * 2.0f - 1.0f, 0.0f, 0.0f);
        }

        // Upload as texture
        textureManager.createTexture2DFromBuffer(
            "ssaoNoise",
            noiseValues.data(),
            noiseValues.size() * sizeof(glm::vec4),
            VK_FORMAT_R32G32B32A32_SFLOAT,
            SSAO_NOISE_DIM,
            SSAO_NOISE_DIM,
            VK_FILTER_NEAREST,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        
        VkDescriptorImageInfo ssaoNoiseInfo{};
        ssaoNoiseInfo.sampler = textureManager.getSampler("colorSampler");
        ssaoNoiseInfo.imageView = textureManager.getTexture("ssaoNoise")->view;
        ssaoNoiseInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        // SSAO Params
        passBuffers[static_cast<uint8_t>(PassName::SSAO)].push_back(std::make_shared<ArxBuffer>(
                                                                        arxDevice,
                                                                        sizeof(CompositionParams),
                                                                        1,
                                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
        passBuffers[static_cast<uint8_t>(PassName::SSAO)][1]->map();
        passBuffers[static_cast<uint8_t>(PassName::SSAO)][1]->writeToBuffer(&compParams);
        passBuffers[static_cast<uint8_t>(PassName::SSAO)][1]->unmap();
        
        descriptorSets[static_cast<uint8_t>(PassName::SSAO)].resize(1);
        
        auto ssaoKernelInfo = passBuffers[static_cast<uint8_t>(PassName::SSAO)][0]->descriptorInfo();
        auto ssaoParamsInfo = passBuffers[static_cast<uint8_t>(PassName::SSAO)][1]->descriptorInfo();
        
        ArxDescriptorWriter(*descriptorLayouts[static_cast<uint8_t>(PassName::SSAO)][0],
                            *descriptorPools[static_cast<uint8_t>(PassName::SSAO)])
                            .writeImage(0, &samplerPosDepthInfo)
                            .writeImage(1, &samplerNormalInfo)
                            .writeImage(2, &ssaoNoiseInfo)
                            .writeBuffer(3, &ssaoKernelInfo)
                            .writeBuffer(4, &ssaoParamsInfo)
                            .build(descriptorSets[static_cast<uint8_t>(PassName::SSAO)][0]);
        
        // ====================================================================================
        //                                    SSAOBLUR
        // ====================================================================================
        
        descriptorPools[static_cast<uint8_t>(PassName::SSAOBLUR)] = ArxDescriptorPool::Builder(arxDevice)
                                                                    .setMaxSets(1)
                                                                    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.0f)
                                                                    .build();
        
        VkDescriptorImageInfo samplerSSAOInfo{};
        samplerSSAOInfo.sampler = textureManager.getSampler("colorSampler");
        samplerSSAOInfo.imageView = textureManager.getAttachment("ssaoColor")->view;
        samplerSSAOInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        descriptorSets[static_cast<uint8_t>(PassName::SSAOBLUR)].resize(1);
        
        ArxDescriptorWriter(*descriptorLayouts[static_cast<uint8_t>(PassName::SSAOBLUR)][0],
                            *descriptorPools[static_cast<uint8_t>(PassName::SSAOBLUR)])
                            .writeImage(0, &samplerSSAOInfo)
                            .build(descriptorSets[static_cast<uint8_t>(PassName::SSAOBLUR)][0]);
        
        // ====================================================================================
        //                                     Deferred
        // ====================================================================================
        
        descriptorPools[static_cast<uint8_t>(PassName::DEFERRED)] = ArxDescriptorPool::Builder(arxDevice)
                                                                    .setMaxSets(1)
                                                                    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5.0f)
                                                                    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 4.0f)
                                                                    .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.0f)
                                                                    .build();
        
        VkDescriptorImageInfo samplerAlbedoInfo{};
        samplerAlbedoInfo.sampler = textureManager.getSampler("colorSampler");
        samplerAlbedoInfo.imageView = textureManager.getAttachment("gAlbedo")->view;
        samplerAlbedoInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)].push_back(std::make_shared<ArxBuffer>(
                                                                     arxDevice,
                                                                     sizeof(GlobalUbo),
                                                                     1,
                                                                     VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                     VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
        
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][0]->map();
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][0]->writeToBuffer(&ubo);
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][0]->unmap();
        
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)].push_back(Materials::pointLightBuffer);
        
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)].push_back(std::make_shared<ArxBuffer>(
                                                                    arxDevice,
                                                                    sizeof(uint32_t),
                                                                    1,
                                                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
        
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][2]->map();
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][2]->writeToBuffer(&Materials::maxPointLights);
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][2]->unmap();
        
        
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)].push_back(ClusteredShading::clusterBuffer);
        
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)].push_back(std::make_shared<ArxBuffer>(
                                                                    arxDevice,
                                                                    sizeof(ClusteredShading::Frustum),
                                                                    1,
                                                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT));
//        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][4]->map();
        
        // Create the texture using the LTC1 data
        textureManager.createTexture2DFromBuffer(
            "LTC1_Texture",
            (void*)LTC1,
            sizeof(LTC1),
            VK_FORMAT_R32G32B32A32_SFLOAT,
            64,
            64,
            VK_FILTER_NEAREST,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        
        VkDescriptorImageInfo samplerLTC1Info{};
        samplerLTC1Info.sampler = textureManager.getTexture("LTC1_Texture")->sampler;
        samplerLTC1Info.imageView = textureManager.getTexture("LTC1_Texture")->view;
        samplerLTC1Info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        // Create the texture using the LTC2 data
        textureManager.createTexture2DFromBuffer(
            "LTC2_Texture",
            (void*)LTC2,
            sizeof(LTC2),
            VK_FORMAT_R32G32B32A32_SFLOAT,
            64,
            64,
            VK_FILTER_NEAREST,
            VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        );
        
        VkDescriptorImageInfo samplerLTC2Info{};
        samplerLTC2Info.sampler = textureManager.getTexture("LTC2_Texture")->sampler;
        samplerLTC2Info.imageView = textureManager.getTexture("LTC2_Texture")->view;
        samplerLTC2Info.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)].push_back(Editor::editorDataBuffer);

        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][5]->map();
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][5]->writeToBuffer(&Editor::data);
        
        
        auto uboInfo            = passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][0]->descriptorInfo();
        VkDescriptorBufferInfo pointLightInfo{};
        if (Materials::maxPointLights > 0)
            pointLightInfo      = passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][1]->descriptorInfo();
        auto lightCountInfo     = passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][2]->descriptorInfo();
        auto clusterInfo        = passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][3]->descriptorInfo();
        auto frustumInfo        = passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][4]->descriptorInfo();
        auto editorInfo         = passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][5]->descriptorInfo();
        
        descriptorSets[static_cast<uint8_t>(PassName::DEFERRED)].resize(1);
        
        ArxDescriptorWriter(*descriptorLayouts[static_cast<uint8_t>(PassName::DEFERRED)][0],
                            *descriptorPools[static_cast<uint8_t>(PassName::DEFERRED)])
                            .writeImage(0, &samplerPosDepthInfo)
                            .writeImage(1, &samplerNormalInfo)
                            .writeImage(2, &samplerAlbedoInfo)
                            .writeImage(3, &samplerLTC1Info)
                            .writeImage(4, &samplerLTC2Info)
                            .writeBuffer(5, &uboInfo)
                            .writeBuffer(6, &pointLightInfo)
                            .writeBuffer(7, &clusterInfo)
                            .writeBuffer(8, &lightCountInfo)
                            .writeBuffer(9, &frustumInfo)
                            .writeBuffer(10, &editorInfo)
                            .build(descriptorSets[static_cast<uint8_t>(PassName::DEFERRED)][0]);
        
        // ====================================================================================
        //                                    COMPOSITION
        // ====================================================================================
        
        descriptorPools[static_cast<uint8_t>(PassName::COMPOSITION)] = ArxDescriptorPool::Builder(arxDevice)
                                                                .setMaxSets(1)
                                                                .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6.0f)
                                                                .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.0f)
                                                                .build();
        
        VkDescriptorImageInfo samplerSSAOBlurColorInfo{};
        samplerSSAOBlurColorInfo.sampler = textureManager.getSampler("colorSampler");
        samplerSSAOBlurColorInfo.imageView = textureManager.getAttachment("ssaoBlurColor")->view;
        samplerSSAOBlurColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        VkDescriptorImageInfo samplerDeferredColorInfo{};
        samplerDeferredColorInfo.sampler = textureManager.getSampler("colorSampler");
        samplerDeferredColorInfo.imageView = textureManager.getAttachment("deferredShading")->view;
        samplerDeferredColorInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)].push_back(std::make_shared<ArxBuffer>(
                                                                        arxDevice,
                                                                        sizeof(CompositionParams),
                                                                        1,
                                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
        
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][0]->map();
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][0]->writeToBuffer(&compParams);
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][0]->unmap();


        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)].push_back(Editor::editorDataBuffer);
        
        // Shared buffer already mapped
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][1]->writeToBuffer(&Editor::data);
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][1]->unmap();
        
        
        descriptorSets[static_cast<uint8_t>(PassName::COMPOSITION)].resize(1);
        
        ArxDescriptorWriter(*descriptorLayouts[static_cast<uint8_t>(PassName::COMPOSITION)][0],
                            *descriptorPools[static_cast<uint8_t>(PassName::COMPOSITION)])
                            .writeImage(0, &samplerPosDepthInfo)
                            .writeImage(1, &samplerNormalInfo)
                            .writeImage(2, &samplerAlbedoInfo)
                            .writeImage(3, &samplerSSAOInfo)
                            .writeImage(4, &samplerSSAOBlurColorInfo)
                            .writeImage(5, &samplerDeferredColorInfo)
                            .writeBuffer(6, &ssaoParamsInfo)
                            .writeBuffer(7, &editorInfo)
                            .build(descriptorSets[static_cast<uint8_t>(PassName::COMPOSITION)][0]);

        // ====================================================================================
        //                                       IMGUI
        // ====================================================================================

        descriptorPools[static_cast<uint8_t>(PassName::IMGUI)] = ArxDescriptorPool::Builder(arxDevice)
                                                                    .setMaxSets(1)
                                                                    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.f)
                                                                    .build();


        descriptorSets[static_cast<uint8_t>(PassName::IMGUI)].resize(1);


        VkDescriptorImageInfo imguiViewport{};
        imguiViewport.sampler = textureManager.getSampler("colorSampler");
        imguiViewport.imageView = textureManager.getAttachment("composition")->view;
        imguiViewport.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

        ArxDescriptorWriter(*descriptorLayouts[static_cast<uint8_t>(PassName::IMGUI)][0],
                            *descriptorPools[static_cast<uint8_t>(PassName::IMGUI)])
                            .writeImage(0, &imguiViewport)
                            .build(descriptorSets[static_cast<uint8_t>(PassName::IMGUI)][0]);
    }

    void ArxRenderer::cleanupResources() {
        for (VkPipelineLayout layout : pipelineLayouts) {
            if (layout != VK_NULL_HANDLE) {
                vkDestroyPipelineLayout(arxDevice.device(), layout, nullptr);
            }
        }
        
        for (auto& sets : descriptorSets) {
            sets.clear();
        }

        for (auto& pool : descriptorPools) {
            if (pool) {
                pool->resetPool();
                pool.reset();
            }
        }

        for (auto& layoutList : descriptorLayouts) {
            for (auto& layout : layoutList) {
                layout.reset();
            }
            layoutList.clear();
        }

        pipelines[static_cast<uint8_t>(PassName::COMPOSITION)].reset();
        pipelines[static_cast<uint8_t>(PassName::GPass)].reset();
        
        // Manually set the vertex shader module to nullptr to skip the destruction
        // These pipelines are using the fullscreen vertex shader of COMPOSITION
        pipelines[static_cast<uint8_t>(PassName::SSAO)]->resetVertShaderModule();
        pipelines[static_cast<uint8_t>(PassName::SSAO)].reset();
        
        pipelines[static_cast<uint8_t>(PassName::SSAOBLUR)]->resetVertShaderModule();
        pipelines[static_cast<uint8_t>(PassName::SSAOBLUR)].reset();
        
        pipelines[static_cast<uint8_t>(PassName::DEFERRED)]->resetVertShaderModule();
        pipelines[static_cast<uint8_t>(PassName::DEFERRED)].reset();

        pipelines[static_cast<uint8_t>(PassName::IMGUI)]->resetVertShaderModule();
        pipelines[static_cast<uint8_t>(PassName::IMGUI)].reset();

        for (auto& passBufferEntry : passBuffers) {
            for (auto& buffer : passBufferEntry.second) {
                buffer.reset();
            }
            passBufferEntry.second.clear();
        }

        descriptorLayouts.fill({});
        descriptorPools.fill(nullptr);
        descriptorSets.fill({});
        pipelineLayouts.fill(VK_NULL_HANDLE);
        pipelines.fill(nullptr);
        
        passBuffers.clear();
    }


    // Function to create render passes
    void ArxRenderer::createRenderPasses() {
        VkFormat depthFormat = arxSwapChain->findDepthFormat();

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
            attachmentDescs[0].format = VK_FORMAT_R32G32B32A32_SFLOAT;
            attachmentDescs[1].format = VK_FORMAT_R8G8B8A8_UNORM;
            attachmentDescs[2].format = VK_FORMAT_R8G8B8A8_UNORM;
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
        }
        
        // Deferred
        {
            VkAttachmentDescription attachmentDescription = {};
            attachmentDescription.format = VK_FORMAT_B8G8R8A8_SRGB;
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
            dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[0].srcAccessMask = 0;
            dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            dependencies[1].srcSubpass = 0;
            dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[1].dstAccessMask = 0;
            dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            VkRenderPassCreateInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.pAttachments = &attachmentDescription;
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
            renderPassInfo.pDependencies = dependencies.data();

            rpManager.createRenderPass("Deferred", renderPassInfo);
        }
        // Composition
        {
            VkAttachmentDescription attachmentDescription = {};
            attachmentDescription.format = arxSwapChain->getSwapChainImageFormat();
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
            dependencies[0].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[0].srcAccessMask = 0;
            dependencies[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            dependencies[1].srcSubpass = 0;
            dependencies[1].dstSubpass = VK_SUBPASS_EXTERNAL;
            dependencies[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependencies[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
            dependencies[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            dependencies[1].dstAccessMask = 0;
            dependencies[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

            VkRenderPassCreateInfo renderPassInfo = {};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.pAttachments = &attachmentDescription;
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = static_cast<uint32_t>(dependencies.size());
            renderPassInfo.pDependencies = dependencies.data();

            rpManager.createRenderPass("Composition", renderPassInfo);
        }

    }

    // Function to create framebuffers
    void ArxRenderer::createFramebuffers() {
        VkExtent2D swapChainExtent = arxSwapChain->getSwapChainExtent();

        // Create attachments
        textureManager.createAttachment("gPosDepth", swapChainExtent.width, swapChainExtent.height,
                                        VK_FORMAT_R32G32B32A32_SFLOAT,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        textureManager.createAttachment("gNormals", swapChainExtent.width, swapChainExtent.height,
                                        VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        textureManager.createAttachment("gAlbedo", swapChainExtent.width, swapChainExtent.height,
                                        VK_FORMAT_R8G8B8A8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        textureManager.createAttachment("gDepth", swapChainExtent.width, swapChainExtent.height,
                                        arxSwapChain->findDepthFormat(),
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        
        textureManager.createAttachment("ssaoColor", swapChainExtent.width, swapChainExtent.height,
                                        VK_FORMAT_R8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        textureManager.createAttachment("ssaoBlurColor", swapChainExtent.width, swapChainExtent.height,
                                        VK_FORMAT_R8_UNORM,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
        
        textureManager.createAttachment("deferredShading", swapChainExtent.width, swapChainExtent.height,
                                        VK_FORMAT_B8G8R8A8_SRGB,
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);

                                        // Off-screen composition attachment
        textureManager.createAttachment("composition", swapChainExtent.width, swapChainExtent.height,
                                        arxSwapChain->getSwapChainImageFormat(),
                                        VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT);


        // G-Buffer framebuffer
        {
            std::array<VkImageView, 4> attachments;
            attachments[0] = textureManager.getAttachment("gPosDepth")->view;
            attachments[1] = textureManager.getAttachment("gNormals")->view;
            attachments[2] = textureManager.getAttachment("gAlbedo")->view;
            attachments[3] = textureManager.getAttachment("gDepth")->view;
            
            rpManager.createFramebuffer("GBuffer", attachments, swapChainExtent.width, swapChainExtent.height);
        }

        // SSAO framebuffer
        {
            std::array<VkImageView, 1> attachments;
            attachments[0] = textureManager.getAttachment("ssaoColor")->view;
            
            rpManager.createFramebuffer("SSAO", attachments, swapChainExtent.width, swapChainExtent.height);
        }

        // SSAO Blur framebuffer
        {
            std::array<VkImageView, 1> attachments;
            attachments[0] = textureManager.getAttachment("ssaoBlurColor")->view;
            
            rpManager.createFramebuffer("SSAOBlur", attachments, swapChainExtent.width, swapChainExtent.height);
        }

        // Deferred framebuffer
        {
            std::array<VkImageView, 1> attachments;
            attachments[0] = textureManager.getAttachment("deferredShading")->view;

            rpManager.createFramebuffer("Deferred", attachments, swapChainExtent.width, swapChainExtent.height);
        }
        // Composition framebuffer
        {
            std::array<VkImageView, 1> attachments;
            attachments[0] = textureManager.getAttachment("composition")->view;

            rpManager.createFramebuffer("Composition", attachments, swapChainExtent.width, swapChainExtent.height);
        }

        // Shared sampler used for all color attachments
        textureManager.createSampler("colorSampler");
    }

    void ArxRenderer::Passes(FrameInfo &frameInfo, Editor& editor) {
        // ====================================================================================
        //                                      G-Buffer
        // ====================================================================================
        Profiler::startStageTimer("G-Pass", Profiler::Type::GPU, frameInfo.commandBuffer);

        beginRenderPass(frameInfo.commandBuffer, "GBuffer");
        
        pipelines[static_cast<uint8_t>(PassName::GPass)]->bind(frameInfo.commandBuffer);
    
        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayouts[static_cast<uint8_t>(PassName::GPass)],
                                0,
                                1,
                                &descriptorSets[static_cast<uint8_t>(PassName::GPass)][0],
                                0,
                                nullptr);
        
        frameInfo.voxel[0].model->bind(frameInfo.commandBuffer);
        
        PushConstantData push{};

        frameInfo.voxel[0].transform.scale = glm::vec3(VOXEL_SIZE);
        push.modelMatrix    = frameInfo.voxel[0].transform.mat4();
        push.normalMatrix   = frameInfo.voxel[0].transform.normalMatrix();

        vkCmdPushConstants(frameInfo.commandBuffer,
                           pipelineLayouts[static_cast<uint8_t>(PassName::GPass)],
                           VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(PushConstantData),
                           &push);
        
        uint32_t drawCount = BufferManager::readDrawCommandCount();
        
        if (drawCount > 0) {
            vkCmdDrawIndexedIndirect(frameInfo.commandBuffer,
                                     BufferManager::drawIndirectBuffer->getBuffer(),
                                     0,
                                     drawCount,
                                     sizeof(GPUIndirectDrawCommand));
        }
        
        vkCmdEndRenderPass(frameInfo.commandBuffer);
        Profiler::stopStageTimer("G-Pass", Profiler::Type::GPU, frameInfo.commandBuffer);
        
        // ====================================================================================
        //                                      SSAO
        // ====================================================================================
        
        if (compParams.ssao) {
            Profiler::startStageTimer("SSAO", Profiler::Type::GPU, frameInfo.commandBuffer);
            beginRenderPass(frameInfo.commandBuffer, "SSAO");
            
            pipelines[static_cast<uint8_t>(PassName::SSAO)]->bind(frameInfo.commandBuffer);
            
            vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayouts[static_cast<uint8_t>(PassName::SSAO)],
                                    0,
                                    1,
                                    &descriptorSets[static_cast<uint8_t>(PassName::SSAO)][0],
                                    0,
                                    nullptr);
            
            vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
            vkCmdEndRenderPass(frameInfo.commandBuffer);
            Profiler::stopStageTimer("SSAO", Profiler::Type::GPU, frameInfo.commandBuffer);
        }
        
        // ====================================================================================
        //                                    SSAOBlur
        // ====================================================================================
        
        if ((compParams.ssao && compParams.ssaoBlur && !compParams.ssaoOnly)) {
            Profiler::startStageTimer("SSAOBlur", Profiler::Type::GPU, frameInfo.commandBuffer);
            
            beginRenderPass(frameInfo.commandBuffer, "SSAOBlur");
            
            pipelines[static_cast<uint8_t>(PassName::SSAOBLUR)]->bind(frameInfo.commandBuffer);
            
            vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayouts[static_cast<uint8_t>(PassName::SSAOBLUR)],
                                    0,
                                    1,
                                    &descriptorSets[static_cast<uint8_t>(PassName::SSAOBLUR)][0],
                                    0,
                                    nullptr);
            
            vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
            vkCmdEndRenderPass(frameInfo.commandBuffer);
            Profiler::stopStageTimer("SSAOBlur", Profiler::Type::GPU, frameInfo.commandBuffer);
        }
        
        // ====================================================================================
        //                                     Deferred
        // ====================================================================================
        
        if (compParams.deferred) {
            Profiler::startStageTimer("FrustumCluster", Profiler::Type::GPU, frameInfo.commandBuffer);
            ClusteredShading::dispatchComputeFrustumCluster(frameInfo.commandBuffer);
            Profiler::stopStageTimer("FrustumCluster", Profiler::Type::GPU, frameInfo.commandBuffer);

            Profiler::startStageTimer("CullLights", Profiler::Type::GPU, frameInfo.commandBuffer);
            ClusteredShading::dispatchComputeClusterCulling(frameInfo.commandBuffer);
            Profiler::stopStageTimer("CullLights", Profiler::Type::GPU, frameInfo.commandBuffer);

            Profiler::startStageTimer("Deferred Shading", Profiler::Type::GPU, frameInfo.commandBuffer);
            beginRenderPass(frameInfo.commandBuffer, "Deferred");
            
            pipelines[static_cast<uint8_t>(PassName::DEFERRED)]->bind(frameInfo.commandBuffer);
            
            vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                    VK_PIPELINE_BIND_POINT_GRAPHICS,
                                    pipelineLayouts[static_cast<uint8_t>(PassName::DEFERRED)],
                                    0,
                                    1,
                                    &descriptorSets[static_cast<uint8_t>(PassName::DEFERRED)][0],
                                    0,
                                    nullptr);
            
            vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
            vkCmdEndRenderPass(frameInfo.commandBuffer);
            Profiler::stopStageTimer("Deferred Shading", Profiler::Type::GPU, frameInfo.commandBuffer);
        }
                
        // ====================================================================================
        //                                   COMPOSITION
        // ====================================================================================

        Profiler::startStageTimer("Frame Composition", Profiler::Type::GPU, frameInfo.commandBuffer);
        beginRenderPass(frameInfo.commandBuffer, "Composition");
        
        pipelines[static_cast<uint8_t>(PassName::COMPOSITION)]->bind(frameInfo.commandBuffer);
    
        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayouts[static_cast<uint8_t>(PassName::COMPOSITION)],
                                0,
                                1,
                                &descriptorSets[static_cast<uint8_t>(PassName::COMPOSITION)][0],
                                0,
                                nullptr);
        
        vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
        endSwapChainRenderPass(frameInfo.commandBuffer);
        Profiler::stopStageTimer("Frame Composition", Profiler::Type::GPU, frameInfo.commandBuffer);

        // ====================================================================================
        //                                   IMGUI Viewport
        // ====================================================================================

        beginSwapChainRenderPass(frameInfo.commandBuffer);
        pipelines[static_cast<uint8_t>(PassName::IMGUI)]->bind(frameInfo.commandBuffer);

        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayouts[static_cast<uint8_t>(PassName::IMGUI)],
                                0,
                                1,
                                &descriptorSets[static_cast<uint8_t>(PassName::IMGUI)][0],
                                0,
                                nullptr);

        vkCmdDraw(frameInfo.commandBuffer, 3, 1, 0, 0);
        editor.render(frameInfo.commandBuffer, descriptorSets[static_cast<uint8_t>(PassName::IMGUI)][0]);
        endSwapChainRenderPass(frameInfo.commandBuffer);
    }

    void ArxRenderer::updateUniforms(const GlobalUbo &rhs, const CompositionParams &params) {
        ubo.projection  = rhs.projection;
        ubo.view        = rhs.view;
        ubo.inverseView = rhs.inverseView;
        ubo.zNear       = rhs.zNear;
        ubo.zFar        = rhs.zFar;
        
        // G-Buffer
        passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->map();
        passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->writeToBuffer(&ubo);
        passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->unmap();
        
        // SSAO
        compParams.projection    = rhs.projection;
        compParams.view          = rhs.view;
        compParams.inverseView   = rhs.inverseView;
        compParams.ssao          = params.ssao;
        compParams.ssaoOnly      = params.ssaoOnly;
        compParams.ssaoBlur      = params.ssaoBlur;
        compParams.deferred      = params.deferred;
        
        passBuffers[static_cast<uint8_t>(PassName::SSAO)][1]->map();
        passBuffers[static_cast<uint8_t>(PassName::SSAO)][1]->writeToBuffer(&compParams);
        passBuffers[static_cast<uint8_t>(PassName::SSAO)][1]->unmap();
        
        // Deferred
        ClusteredShading::Frustum frustumParams;
        frustumParams.gridSize = glm::uvec3(ClusteredShading::gridSizeX, ClusteredShading::gridSizeY, ClusteredShading::gridSizeZ);
        frustumParams.screenDimensions = glm::uvec2(arxSwapChain->getSwapChainExtent().width, arxSwapChain->getSwapChainExtent().height);
        frustumParams.zFar = rhs.zFar;
        frustumParams.zNear = rhs.zNear;
        
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][0]->map();
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][4]->map();
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][0]->writeToBuffer(&ubo);
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][4]->writeToBuffer(&frustumParams);
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][0]->unmap();
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][4]->unmap();

        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][5]->map();
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][5]->writeToBuffer(&Editor::data);
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][5]->unmap();
        
        // COMPOSITION
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][0]->map();
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][0]->writeToBuffer(&compParams);
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][0]->unmap();

        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][1]->map();
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][1]->writeToBuffer(&Editor::data);
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][1]->unmap();
    }

    void ArxRenderer::init_Passes() {
        createRenderPasses();
        createFramebuffers();
        createDescriptorSetLayouts();
        createPipelineLayouts();
        createPipelines();
        createUniformBuffers();
        
        // Initialize culling resources after the G-Pass has created the depth texture
        arxSwapChain->Init_OcclusionCulling();
    }
}
