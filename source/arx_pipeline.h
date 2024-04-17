#pragma once

#include "arx_device.h"

#include <string>
#include <vector>

namespace arx {

    struct PipelineConfigInfo {
        PipelineConfigInfo(const PipelineConfigInfo&) = delete;
        PipelineConfigInfo& operator=(const PipelineConfigInfo&) = delete;
        PipelineConfigInfo() = default;
        
        std::vector<VkVertexInputBindingDescription> bindingDescriptions{};
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        VkPipelineViewportStateCreateInfo viewportInfo;
        VkPipelineInputAssemblyStateCreateInfo inputAssemblyInfo;
        VkPipelineRasterizationStateCreateInfo rasterizationInfo;
        VkPipelineMultisampleStateCreateInfo multisampleInfo;
        VkPipelineColorBlendAttachmentState colorBlendAttachment;
        VkPipelineColorBlendStateCreateInfo colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        std::vector<VkDynamicState> dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
        
        VkPipelineShaderStageCreateInfo computeShaderStage{};
    };

    class ArxPipeline {
    public:
        ArxPipeline(ArxDevice& device,
                    const std::string& vertFilepath,
                    const std::string& fragFilepath,
                    const PipelineConfigInfo& config);
        
        ArxPipeline(ArxDevice& device,
                    const std::string& compFilepath,
                    VkPipelineLayout& pipelineLayout);
        
        ~ArxPipeline();
        
        ArxPipeline(const ArxPipeline&) = delete;
        ArxPipeline& operator=(const ArxPipeline&) = delete;
        
        void bind(VkCommandBuffer commandBuffer);
        static void defaultPipelineConfigInfo(VkSampleCountFlagBits msaaSamples, PipelineConfigInfo& configInfo);
        static void enableAlphaBlending(PipelineConfigInfo& configInfo);
        void createComputePipeline(const std::string &compFilepath, VkPipelineLayout& pipelineLayout);
        VkPipeline      computePipeline;
        VkShaderModule  computeShaderModule;
    private:
        static std::vector<char> readFile(const std::string& filepath);
        
        void createGraphicsPipeline(const std::string& vertFilepath,
                                    const std::string& fragFilepath,
                                    const PipelineConfigInfo& config);
        
        void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);
        
        // Aggregation for implicit relationship only
        // Will outlive any instances of the containing class that depend on it
        // A pipeline needs a device to exist
        ArxDevice& arxDevice;
        VkPipeline graphicsPipeline;
        VkShaderModule vertShaderModule;
        VkShaderModule fragShaderModule;
    };
}
