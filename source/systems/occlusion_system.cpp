#include "../engine_pch.hpp"

#include "occlusion_system.hpp"

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
                        .addBinding(6, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                        .addBinding(7, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                        .addBinding(8, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
                        .build();
            
        createCullingPipelineLayout();
        createCullingPipeline();
        
        // late
        createEarlyCullingPipelineLayout();
        createEarlyCullingPipeline();
    }

    OcclusionSystem::~OcclusionSystem() {
        vkDestroyPipelineLayout(arxDevice.device(), depthPyramidPipelineLayout, nullptr);
        vkDestroyPipelineLayout(arxDevice.device(), cullingPipelineLayout, nullptr);
        vkDestroyPipelineLayout(arxDevice.device(), earlyCullingPipelineLayout, nullptr);
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
        
        cullingPipeline = std::make_unique<ArxPipeline>(arxDevice,
                                                        "shaders/occlusion_culling.spv",
                                                        cullingPipelineLayout);
    }

    void OcclusionSystem::createEarlyCullingPipelineLayout() {
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{cullingDescriptorLayout->getDescriptorSetLayout()};
        
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts              = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount   = 0;
        
        if (vkCreatePipelineLayout(arxDevice.device(), &pipelineLayoutInfo, nullptr, &earlyCullingPipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create late culling pipeline layout!");
        }
    }

    void OcclusionSystem::createEarlyCullingPipeline() {
        assert(earlyCullingPipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");
        
        earlyCullingPipeline = std::make_unique<ArxPipeline>(arxDevice,
                                                        "shaders/occlusionEarly_culling.spv",
                                                        earlyCullingPipelineLayout);
    }
}
