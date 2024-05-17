#include <stdio.h>
#include "simple_render_system.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <stdexcept>
#include <array>
#include <cassert>

namespace arx {
    
    struct SimplePushConstantData {
        glm::mat4 modelMatrix{1.f};
        glm::mat4 normalMatrix{1.f};
    };

    SimpleRenderSystem::SimpleRenderSystem(ArxDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
        : arxDevice{device} {
        createPipelineLayout(globalSetLayout);
        createPipeline(renderPass);
    }

    SimpleRenderSystem::~SimpleRenderSystem() {
        vkDestroyPipelineLayout(arxDevice.device(), pipelineLayout, nullptr);
    }
    
    void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
        
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags    = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset        = 0;
        pushConstantRange.size          = sizeof(SimplePushConstantData);
        
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};
        
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts              = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount   = 1;
        pipelineLayoutInfo.pPushConstantRanges      = &pushConstantRange;
        
        if (vkCreatePipelineLayout(arxDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }

    void SimpleRenderSystem::createPipeline(VkRenderPass renderPass) {
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");
        
        PipelineConfigInfo pipelineConfig{};
        VkPipelineColorBlendAttachmentState attachment1 = ArxPipeline::createDefaultColorBlendAttachment();
        pipelineConfig.colorBlendAttachments = {attachment1};
        ArxPipeline::defaultPipelineConfigInfo(pipelineConfig);
        pipelineConfig.multisampleInfo.rasterizationSamples = arxDevice.msaaSamples;
        pipelineConfig.renderPass                           = renderPass;
        pipelineConfig.pipelineLayout                       = pipelineLayout;
        arxPipeline = std::make_unique<ArxPipeline>(arxDevice,
                                                    "shaders/vert.spv",
                                                    "shaders/frag.spv",
                                                    pipelineConfig);
    }

    void SimpleRenderSystem::renderGameObjects(FrameInfo &frameInfo, std::vector<uint32_t> &visibleChunksIndices) {
        arxPipeline->bind(frameInfo.commandBuffer);
        
        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout,
                                0, 1,
                                &frameInfo.globalDescriptorSet,
                                0,
                                nullptr);
        
        for (auto i : visibleChunksIndices) {
            if (frameInfo.voxel[i].model == nullptr) continue;
            SimplePushConstantData push{};
            frameInfo.voxel[i].transform.scale = glm::vec3(VOXEL_SIZE/2);
            push.modelMatrix    = frameInfo.voxel[i].transform.mat4();
            push.normalMatrix   = frameInfo.voxel[i].transform.normalMatrix();


            vkCmdPushConstants(frameInfo.commandBuffer,
                               pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT |
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(SimplePushConstantData),
                               &push);
            
            frameInfo.voxel[i].model->bind(frameInfo.commandBuffer);
            frameInfo.voxel[i].model->draw(frameInfo.commandBuffer);
        }
//        arxPipeline->bind(frameInfo.commandBuffer);
//
//        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
//                                VK_PIPELINE_BIND_POINT_GRAPHICS,
//                                pipelineLayout,
//                                0, 1,
//                                &frameInfo.globalDescriptorSet,
//                                0,
//                                nullptr);
//
//        // Bind all buffers using the static helper function
//        arx::BufferManager::bindBuffers(frameInfo.commandBuffer);
//
//        // Perform indirect drawing
//        vkCmdDrawIndexedIndirect(frameInfo.commandBuffer, arx::BufferManager::drawIndirectBuffer->getBuffer(), 0, static_cast<uint32_t>(visibleChunksIndices.size()), sizeof(VkDrawIndexedIndirectCommand));
    }
}
