#include <stdio.h>
#include "arx_renderer.h"


#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

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
            SSAOBLUR,
            DEFERRED,
            COMPOSITION,
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
                                          .addBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // UBO
                                          .addBinding(4, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_FRAGMENT_BIT) // PointLightsBuffer
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
            throw std::runtime_error("failed to create gpass pipeline layout!");
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
            throw std::runtime_error("failed to create ssao pipeline layout!");
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
            throw std::runtime_error("failed to create ssaoblur pipeline layout!");
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
            throw std::runtime_error("failed to create deferred pipeline layout!");
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
            throw std::runtime_error("failed to create composition pipeline layout!");
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
        compositionConfigInfo.renderPass                    = arxSwapChain->getRenderPass();
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
                                                            pipelines[static_cast<uint8_t>(PassName::COMPOSITION)]->getVertShaderModule(),
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
                                                                pipelines[static_cast<uint8_t>(PassName::COMPOSITION)]->getVertShaderModule(),
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
                                                                pipelines[static_cast<uint8_t>(PassName::COMPOSITION)]->getVertShaderModule(),
                                                                "shaders/deferred.spv",
                                                                deferredConfigInfo);
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
                                                                        sizeof(uboSSAOParams),
                                                                        1,
                                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
        passBuffers[static_cast<uint8_t>(PassName::SSAO)][1]->map();
        passBuffers[static_cast<uint8_t>(PassName::SSAO)][1]->writeToBuffer(&uboSSAOParams);
        
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
                                                                    .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 3.0f)
                                                                    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f)
                                                                    .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1.0f)
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
        
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)].push_back(Materials::pointLightBuffer);
        
        auto uboInfo        = passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][0]->descriptorInfo();
        auto pointLightInfo = passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][1]->descriptorInfo();
        
        descriptorSets[static_cast<uint8_t>(PassName::DEFERRED)].resize(1);
        
        ArxDescriptorWriter(*descriptorLayouts[static_cast<uint8_t>(PassName::DEFERRED)][0],
                            *descriptorPools[static_cast<uint8_t>(PassName::DEFERRED)])
                            .writeImage(0, &samplerPosDepthInfo)
                            .writeImage(1, &samplerNormalInfo)
                            .writeImage(2, &samplerAlbedoInfo)
                            .writeBuffer(3, &uboInfo)
                            .writeBuffer(4, &pointLightInfo)
                            .build(descriptorSets[static_cast<uint8_t>(PassName::DEFERRED)][0]);
        
        // ====================================================================================
        //                                    COMPOSITION
        // ====================================================================================
        
        descriptorPools[static_cast<uint8_t>(PassName::COMPOSITION)] = ArxDescriptorPool::Builder(arxDevice)
                                                                .setMaxSets(1)
                                                                .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 6.0f)
                                                                .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f)
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
                                                                        sizeof(uboSSAOParams),
                                                                        1,
                                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                                                        VK_MEMORY_PROPERTY_HOST_COHERENT_BIT));
        
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][0]->map();
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][0]->writeToBuffer(&uboSSAOParams);
        
        
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
                            .build(descriptorSets[static_cast<uint8_t>(PassName::COMPOSITION)][0]);
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
        
        textureManager.createAttachment("deferredShading", arxSwapChain->width(), arxSwapChain->height(),
                                        VK_FORMAT_B8G8R8A8_SRGB,
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
        
        // Deferred
        {
            VkAttachmentDescription attachmentDescription = {};
            attachmentDescription.format = textureManager.getAttachment("deferredShading")->format;
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

            std::array<VkImageView, 1> attachments;
            attachments[0] = textureManager.getAttachment("deferredShading")->view;

            rpManager.createFramebuffer("Deferred", attachments, arxSwapChain->width(), arxSwapChain->height());
        }
    
        // Shared sampler used for all color attachments
        textureManager.createSampler("colorSampler");
    }

    void ArxRenderer::Passes(FrameInfo &frameInfo) {
        // ====================================================================================
        //                                      G-Buffer
        // ====================================================================================
        
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

        frameInfo.voxel[0].transform.scale = glm::vec3(VOXEL_SIZE*0.5);
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
        
        // ====================================================================================
        //                                      SSAO
        // ====================================================================================
        
        if (uboSSAOParams.ssao) {
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
        }
        
        // ====================================================================================
        //                                    SSAOBlur
        // ====================================================================================
        
        if ((uboSSAOParams.ssao && uboSSAOParams.ssaoBlur && !uboSSAOParams.ssaoOnly)) {
            
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
        }
        
        // ====================================================================================
        //                                     Deferred
        // ====================================================================================
        
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
                
        // ====================================================================================
        //                                   COMPOSITION
        // ====================================================================================

        beginSwapChainRenderPass(frameInfo.commandBuffer);
        
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
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frameInfo.commandBuffer);
        endSwapChainRenderPass(frameInfo.commandBuffer);
    }

    void ArxRenderer::updateMisc(const GlobalUbo &rhs, const SSAOParams &ssaorhs) {
        ubo.projection  = rhs.projection;
        ubo.view        = rhs.view;
        ubo.inverseView = rhs.inverseView;
        ubo.zNear       = rhs.zNear;
        ubo.zFar        = rhs.zFar;
        
        // G-Buffer
        passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->writeToBuffer(&ubo);
        passBuffers[static_cast<uint8_t>(PassName::GPass)][0]->flush();
        
        // SSAO
        uboSSAOParams.projection    = rhs.projection;
        uboSSAOParams.view          = rhs.view;
        uboSSAOParams.inverseView   = rhs.inverseView;
        uboSSAOParams.ssao          = ssaorhs.ssao;
        uboSSAOParams.ssaoOnly      = ssaorhs.ssaoOnly;
        uboSSAOParams.ssaoBlur      = ssaorhs.ssaoBlur;
        
        passBuffers[static_cast<uint8_t>(PassName::SSAO)][1]->writeToBuffer(&uboSSAOParams);
        
        // Deferred
        passBuffers[static_cast<uint8_t>(PassName::DEFERRED)][0]->writeToBuffer(&ubo);
        
        // COMPOSITION
        passBuffers[static_cast<uint8_t>(PassName::COMPOSITION)][0]->writeToBuffer(&uboSSAOParams);
    }

    void ArxRenderer::init_Passes() {
        createRenderPasses();
        createDescriptorSetLayouts();
        createPipelineLayouts();
        createPipelines();
        createUniformBuffers();
        
        // Initialize culling resources after G-Pass created the depth texture
        arxSwapChain->Init_OcclusionCulling();
    }
}
