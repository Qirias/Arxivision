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
        ArxPipeline::defaultPipelineConfigInfo(arxDevice.msaaSamples, pipelineConfig);
        pipelineConfig.renderPass       = renderPass;
        pipelineConfig.pipelineLayout   = pipelineLayout;
        arxPipeline = std::make_unique<ArxPipeline>(arxDevice,
                                                    "shaders/vert.spv",
                                                    "shaders/frag.spv",
                                                    pipelineConfig);
    }

//    void printMat4(const glm::mat4& mat) {
//        std::cout << "[ " << mat[0][0] << " " << mat[0][1] << " " << mat[0][2] << " " << mat[0][3] << " ]" << std::endl;
//        std::cout << "[ " << mat[1][0] << " " << mat[1][1] << " " << mat[1][2] << " " << mat[1][3] << " ]" << std::endl;
//        std::cout << "[ " << mat[2][0] << " " << mat[2][1] << " " << mat[2][2] << " " << mat[2][3] << " ]" << std::endl;
//        std::cout << "[ " << mat[3][0] << " " << mat[3][1] << " " << mat[3][2] << " " << mat[3][3] << " ]" << std::endl;
//    }
    
    void SimpleRenderSystem::renderGameObjects(FrameInfo &frameInfo) {
        arxPipeline->bind(frameInfo.commandBuffer);
        
        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout,
                                0, 1,
                                &frameInfo.globalDescriptorSet,
                                0,
                                nullptr);
        
        
        SimplePushConstantData push{};
        frameInfo.gameObjects[0].transform.scale = glm::vec3(0.1f);
        push.modelMatrix    = frameInfo.gameObjects[0].transform.mat4();
        push.normalMatrix   = frameInfo.gameObjects[0].transform.normalMatrix();

        
        vkCmdPushConstants(frameInfo.commandBuffer,
                           pipelineLayout,
                           VK_SHADER_STAGE_VERTEX_BIT |
                           VK_SHADER_STAGE_FRAGMENT_BIT,
                           0,
                           sizeof(SimplePushConstantData),
                           &push);
        
        frameInfo.gameObjects[0].model->bind(frameInfo.commandBuffer);
        frameInfo.gameObjects[0].model->draw(frameInfo.commandBuffer);
        
//        for (auto& kv : frameInfo.gameObjects) {
//            auto& obj = kv.second;
//            if (obj.model == nullptr) continue;
//            SimplePushConstantData push{};
//            push.modelMatrix    = obj.transform.mat4();
//            push.normalMatrix   = obj.transform.normalMatrix();
//
//            vkCmdPushConstants(frameInfo.commandBuffer,
//                               pipelineLayout,
//                               VK_SHADER_STAGE_VERTEX_BIT |
//                               VK_SHADER_STAGE_FRAGMENT_BIT,
//                               0,
//                               sizeof(SimplePushConstantData),
//                               &push);
//            obj.model->bind(frameInfo.commandBuffer);
//            obj.model->draw(frameInfo.commandBuffer);
//        }
    }
}