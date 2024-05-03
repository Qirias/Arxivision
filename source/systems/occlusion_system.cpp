#include "occlusion_system.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <stdexcept>
#include <array>
#include <cassert>

namespace arx {
    
    struct DepthReduceData {
        glm::vec2 dimensions;
    };

    OcclusionSystem::OcclusionSystem(ArxDevice &device)
        : arxDevice{device} {
        depthDescriptorLayout = ArxDescriptorSetLayout::Builder(arxDevice)
                        .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, VK_SHADER_STAGE_COMPUTE_BIT)
                        .addBinding(1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                        .build();
        
        createDepthPyramidPipelineLayout();
        createDepthPyramidPipeline();
        
        cullingDescriptorLayout = ArxDescriptorSetLayout::Builder(arxDevice)
                        .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                        .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                        .addBinding(2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_COMPUTE_BIT)
                        .addBinding(3, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                        .addBinding(4, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                        .addBinding(5, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                        .build();
            
        createCullingPipelineLayout();
        createCullingPipeline();
        
        // late
//        createLateCullingPipelineLayout();
//        createLateCullingPipeline();
    }

    OcclusionSystem::~OcclusionSystem() {
        vkDestroyPipelineLayout(arxDevice.device(), depthPyramidPipelineLayout, nullptr);
        vkDestroyPipelineLayout(arxDevice.device(), cullingPipelineLayout, nullptr);
        vkDestroyPipelineLayout(arxDevice.device(), lateCullingPipelineLayout, nullptr);
    }
    
    void OcclusionSystem::createDepthPyramidPipelineLayout() {
        
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags    = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset        = 0;
        pushConstantRange.size          = sizeof(DepthReduceData);
        
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{depthDescriptorLayout->getDescriptorSetLayout()};
        
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts              = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount   = 1;
        pipelineLayoutInfo.pPushConstantRanges      = &pushConstantRange;
        
        if (vkCreatePipelineLayout(arxDevice.device(), &pipelineLayoutInfo, nullptr, &depthPyramidPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create depth pipeline layout!");
        }
    }

    void OcclusionSystem::createDepthPyramidPipeline() {
        assert(depthPyramidPipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");
        
        PipelineConfigInfo pipelineConfig{};
        depthPyramidPipeline = std::make_unique<ArxPipeline>(arxDevice,
                                                    "shaders/depth_pyramid.spv",
                                                    depthPyramidPipelineLayout);
    }

    void OcclusionSystem::createCullingPipelineLayout() {
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{cullingDescriptorLayout->getDescriptorSetLayout()};
        
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts              = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount   = 0;
        
        if (vkCreatePipelineLayout(arxDevice.device(), &pipelineLayoutInfo, nullptr, &cullingPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create culling pipeline layout!");
        }
    }

    void OcclusionSystem::createCullingPipeline() {
        assert(cullingPipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");
        
        PipelineConfigInfo pipelineConfig{};
        cullingPipeline = std::make_unique<ArxPipeline>(arxDevice,
                                                        "shaders/occlusion_culling.spv",
                                                        cullingPipelineLayout);
    }

    void OcclusionSystem::createLateCullingPipelineLayout() {
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{cullingDescriptorLayout->getDescriptorSetLayout()};
        
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts              = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount   = 0;
        
        if (vkCreatePipelineLayout(arxDevice.device(), &pipelineLayoutInfo, nullptr, &lateCullingPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create late culling pipeline layout!");
        }
    }

    void OcclusionSystem::createLateCullingPipeline() {
        assert(lateCullingPipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");
        
        PipelineConfigInfo pipelineConfig{};
        lateCullingPipeline = std::make_unique<ArxPipeline>(arxDevice,
                                                        "shaders/occlusionLate_culling.spv",
                                                        lateCullingPipelineLayout);
    }
}


