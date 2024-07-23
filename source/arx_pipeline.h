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
        std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments;
        VkPipelineColorBlendStateCreateInfo colorBlendInfo;
        VkPipelineDepthStencilStateCreateInfo depthStencilInfo;
        std::vector<VkDynamicState> dynamicStateEnables;
        VkPipelineDynamicStateCreateInfo dynamicStateInfo;
        VkPipelineLayout pipelineLayout = nullptr;
        VkRenderPass renderPass = nullptr;
        uint32_t subpass = 0;
        bool useVertexInputState = true; // false for fullscreen triangle
        
        VkPipelineShaderStageCreateInfo vertexShaderStage{};
        VkPipelineShaderStageCreateInfo fragmentShaderStage{};
        VkPipelineShaderStageCreateInfo computeShaderStage{};
    };

    class ArxPipeline {
    public:
        ArxPipeline(ArxDevice& device,
                    const std::string& vertFilepath,
                    const std::string& fragFilepath,
                    const std::string &geomFilepath,
                    const PipelineConfigInfo& config);
        
        ArxPipeline(ArxDevice& device,
                    VkShaderModule vertShaderModule,
                    const std::string& fragFilepath,
                    const PipelineConfigInfo& configInfo);
        
        ArxPipeline(ArxDevice& device,
                    const std::string& compFilepath,
                    VkPipelineLayout& pipelineLayout);
        
        
        ~ArxPipeline();
        
        ArxPipeline(const ArxPipeline&) = delete;
        ArxPipeline& operator=(const ArxPipeline&) = delete;
        
        VkShaderModule getVertShaderModule() const {return vertShaderModule;}
        VkPipelineShaderStageCreateInfo getVertexShaderStageInfo() const {return vertexShaderStageInfo;}
        
        void bind(VkCommandBuffer commandBuffer);
        static void defaultPipelineConfigInfo(PipelineConfigInfo& configInfo);
        static VkPipelineColorBlendAttachmentState createDefaultColorBlendAttachment();
        static void enableAlphaBlending(PipelineConfigInfo& configInfo);
        void createComputePipeline(const std::string &compFilepath, VkPipelineLayout& pipelineLayout);
        
        VkPipeline      computePipeline;
        VkShaderModule  computeShaderModule = VK_NULL_HANDLE;
        
    private:
        static std::vector<char> readFile(const std::string& filepath);
        
        void createGraphicsPipeline(const std::string& vertFilepath,
                                    const std::string& fragFilepath,
                                    const std::string& geomFilepath,
                                    const PipelineConfigInfo& config);
        
        void createGraphicsPipeline(VkShaderModule vertShaderModule,
                                    const std::string& fragFilepath,
                                    const PipelineConfigInfo& configInfo);
        
        void createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule);
        
        // Aggregation for implicit relationship only
        // Will outlive any instances of the containing class that depend on it
        // A pipeline needs a device to exist
        ArxDevice& arxDevice;
        VkPipeline graphicsPipeline;
        VkShaderModule vertShaderModule = VK_NULL_HANDLE;
        VkShaderModule fragShaderModule = VK_NULL_HANDLE;
        VkShaderModule geomShaderModule = VK_NULL_HANDLE;
        
        VkPipelineShaderStageCreateInfo vertexShaderStageInfo{};
    };
}
