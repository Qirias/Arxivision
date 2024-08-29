#include "../source/engine_pch.hpp"

#include "../source/arx_pipeline.h"
#include "../source/arx_model.h"

namespace arx {

    ArxPipeline::ArxPipeline(ArxDevice& device,
                             const std::string& vertFilepath,
                             const std::string& fragFilepath,
                             const std::string& geomFilepath,
                             const PipelineConfigInfo& config) : arxDevice{device} {
        createGraphicsPipeline(vertFilepath, fragFilepath, geomFilepath, config);
    }

    ArxPipeline::ArxPipeline(ArxDevice& device,
                            VkShaderModule vertShaderModule,
                            const std::string& fragFilepath,
                            const PipelineConfigInfo& config) : arxDevice{device}, vertShaderModule{vertShaderModule} {
       createGraphicsPipeline(vertShaderModule, fragFilepath, config);
   }

    ArxPipeline::ArxPipeline(ArxDevice& device,
                             const std::string& compFilepath,
                             VkPipelineLayout& pipelineLayout) : arxDevice{device} {
        createComputePipeline(compFilepath, pipelineLayout);
    }

    ArxPipeline::~ArxPipeline() {
        if (vertShaderModule != VK_NULL_HANDLE)
           vkDestroyShaderModule(arxDevice.device(), vertShaderModule, nullptr);
        if (fragShaderModule != VK_NULL_HANDLE)
           vkDestroyShaderModule(arxDevice.device(), fragShaderModule, nullptr);
        if (computeShaderModule != VK_NULL_HANDLE)
           vkDestroyShaderModule(arxDevice.device(), computeShaderModule, nullptr);
        if (geomShaderModule != VK_NULL_HANDLE)
           vkDestroyShaderModule(arxDevice.device(), geomShaderModule, nullptr);
        if (graphicsPipeline != VK_NULL_HANDLE)
           vkDestroyPipeline(arxDevice.device(), graphicsPipeline, nullptr);
        if (computePipeline != VK_NULL_HANDLE)
            vkDestroyPipeline(arxDevice.device(), computePipeline, nullptr);
    }

    std::vector<char> ArxPipeline::readFile(const std::string& filepath) {
        std::ifstream file(filepath, std::ios::ate | std::ios::binary);
        
        if (!file.is_open()) {
            throw std::runtime_error("failed to open file: " + filepath);
        }
        
        size_t fileSize = static_cast<size_t>(file.tellg());
        std::vector<char> buffer(fileSize);
        
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        
        file.close();
        return buffer;
    }

    void ArxPipeline::createGraphicsPipeline(const std::string &vertFilepath, const std::string &fragFilepath, const std::string &geomFilepath, const PipelineConfigInfo& configInfo) {
        assert(configInfo.pipelineLayout != VK_NULL_HANDLE && "Cannot create graphics pipeline: no pipelineLayout provided in configInfo");
        
        assert(configInfo.renderPass != VK_NULL_HANDLE && "Cannot create graphics pipeline: no renderPass provided in configInfo");

        auto vertCode = readFile(vertFilepath);
        auto fragCode = readFile(fragFilepath);
        
        createShaderModule(vertCode, &vertShaderModule);
        createShaderModule(fragCode, &fragShaderModule);
        
        VkPipelineShaderStageCreateInfo shaderStages[3];
        uint32_t stageCount = 2; // Default without geometry shader
        
        shaderStages[0].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage               = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module              = vertShaderModule;
        shaderStages[0].pName               = "main";
        shaderStages[0].flags               = 0;
        shaderStages[0].pNext               = nullptr;
        shaderStages[0].pSpecializationInfo = configInfo.vertexShaderStage.pSpecializationInfo;
        
        shaderStages[1].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage               = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module              = fragShaderModule;
        shaderStages[1].pName               = "main";
        shaderStages[1].flags               = 0;
        shaderStages[1].pNext               = nullptr;
        shaderStages[1].pSpecializationInfo = configInfo.fragmentShaderStage.pSpecializationInfo;
        
        
        if (!geomFilepath.empty()) {
            auto geomCode = readFile(geomFilepath);
            createShaderModule(geomCode, &geomShaderModule);
            
            shaderStages[2].sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            shaderStages[2].stage               = VK_SHADER_STAGE_GEOMETRY_BIT;
            shaderStages[2].module              = geomShaderModule;
            shaderStages[2].pName               = "main";
            shaderStages[2].flags               = 0;
            shaderStages[2].pNext               = nullptr;
            shaderStages[2].pSpecializationInfo = nullptr;
            
            stageCount = 3; // Update stage count to 3 if geometry shader is provided
        }
        
        auto& bindingDescriptions    = configInfo.bindingDescriptions;
        auto& attributeDescriptions  = configInfo.attributeDescriptions;
        
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        
        if (configInfo.useVertexInputState) {
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
            vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
            vertexInputInfo.pVertexBindingDescriptions = configInfo.bindingDescriptions.data();
            vertexInputInfo.pVertexAttributeDescriptions = configInfo.attributeDescriptions.data();
        } else {
            // Empty VkPipelineVertexInputStateCreateInfo structure
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = nullptr;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr;
        }
        
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType                  = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount             = stageCount;
        pipelineInfo.pStages                = shaderStages;
        pipelineInfo.pVertexInputState      = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState    = &configInfo.inputAssemblyInfo;
        pipelineInfo.pViewportState         = &configInfo.viewportInfo;
        pipelineInfo.pRasterizationState    = &configInfo.rasterizationInfo;
        pipelineInfo.pMultisampleState      = &configInfo.multisampleInfo;
        pipelineInfo.pColorBlendState       = &configInfo.colorBlendInfo;
        pipelineInfo.pDepthStencilState     = &configInfo.depthStencilInfo;
        pipelineInfo.pDynamicState          = &configInfo.dynamicStateInfo;
    
        pipelineInfo.layout                 = configInfo.pipelineLayout;
        pipelineInfo.renderPass             = configInfo.renderPass;
        pipelineInfo.subpass                = configInfo.subpass;
        
        // For optimizing performance by deriving an existing pipeline
//        pipelineInfo.basePipelineIndex      = -1;
//        pipelineInfo.basePipelineHandle     = VK_NULL_HANDLE;
        
        if (vkCreateGraphicsPipelines(arxDevice.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
    }

    void ArxPipeline::createGraphicsPipeline(VkShaderModule vertShaderModule, const std::string &fragFilepath, const PipelineConfigInfo& configInfo) {
        assert(configInfo.pipelineLayout != VK_NULL_HANDLE && "Cannot create graphics pipeline: no pipelineLayout provided in configInfo");

        assert(configInfo.renderPass != VK_NULL_HANDLE && "Cannot create graphics pipeline: no renderPass provided in configInfo");

        VkPipelineShaderStageCreateInfo shaderStages[2];

        // Use existing vertex shader module
        vertexShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertexShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertexShaderStageInfo.module = vertShaderModule;
        vertexShaderStageInfo.pName = "main";
        vertexShaderStageInfo.flags = 0;
        vertexShaderStageInfo.pNext = nullptr;
        vertexShaderStageInfo.pSpecializationInfo = configInfo.vertexShaderStage.pSpecializationInfo;

        shaderStages[0] = vertexShaderStageInfo;

        // Create fragment shader module
        auto fragCode = readFile(fragFilepath);
        createShaderModule(fragCode, &fragShaderModule);

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragShaderModule;
        shaderStages[1].pName = "main";
        shaderStages[1].flags = 0;
        shaderStages[1].pNext = nullptr;
        shaderStages[1].pSpecializationInfo = configInfo.fragmentShaderStage.pSpecializationInfo;

        // Vertex input state
        auto& bindingDescriptions    = configInfo.bindingDescriptions;
        auto& attributeDescriptions  = configInfo.attributeDescriptions;
        
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        
        if (configInfo.useVertexInputState) {
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(attributeDescriptions.size());
            vertexInputInfo.vertexBindingDescriptionCount = static_cast<uint32_t>(bindingDescriptions.size());
            vertexInputInfo.pVertexBindingDescriptions = configInfo.bindingDescriptions.data();
            vertexInputInfo.pVertexAttributeDescriptions = configInfo.attributeDescriptions.data();
        } else {
            // Empty VkPipelineVertexInputStateCreateInfo structure
            vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vertexInputInfo.vertexBindingDescriptionCount = 0;
            vertexInputInfo.vertexAttributeDescriptionCount = 0;
            vertexInputInfo.pVertexBindingDescriptions = nullptr;
            vertexInputInfo.pVertexAttributeDescriptions = nullptr;
        }

        // Graphics pipeline creation
        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &configInfo.inputAssemblyInfo;
        pipelineInfo.pViewportState = &configInfo.viewportInfo;
        pipelineInfo.pRasterizationState = &configInfo.rasterizationInfo;
        pipelineInfo.pMultisampleState = &configInfo.multisampleInfo;
        pipelineInfo.pColorBlendState = &configInfo.colorBlendInfo;
        pipelineInfo.pDepthStencilState = &configInfo.depthStencilInfo;
        pipelineInfo.pDynamicState = &configInfo.dynamicStateInfo;
        pipelineInfo.layout = configInfo.pipelineLayout;
        pipelineInfo.renderPass = configInfo.renderPass;
        pipelineInfo.subpass = configInfo.subpass;

        if (vkCreateGraphicsPipelines(arxDevice.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create graphics pipeline!");
        }
    }

    void ArxPipeline::createShaderModule(const std::vector<char>& code, VkShaderModule* shaderModule) {
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode    = reinterpret_cast<const uint32_t*>(code.data());
        
        if (vkCreateShaderModule(arxDevice.device(), &createInfo, nullptr, shaderModule) != VK_SUCCESS) {
            throw std::runtime_error("failed to create shader module!");
        }
    }

    void ArxPipeline::bind(VkCommandBuffer commandBuffer) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);
    }

    void ArxPipeline::defaultPipelineConfigInfo(PipelineConfigInfo& configInfo) {
        configInfo.inputAssemblyInfo.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        configInfo.inputAssemblyInfo.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        configInfo.inputAssemblyInfo.primitiveRestartEnable = VK_FALSE;
        
        configInfo.viewportInfo.sType           = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        configInfo.viewportInfo.viewportCount   = 1;
        configInfo.viewportInfo.pViewports      = nullptr;
        configInfo.viewportInfo.scissorCount    = 1;
        configInfo.viewportInfo.pScissors       = nullptr;

        configInfo.rasterizationInfo.sType                      = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        configInfo.rasterizationInfo.depthClampEnable           = VK_FALSE;
        configInfo.rasterizationInfo.rasterizerDiscardEnable    = VK_FALSE;
        configInfo.rasterizationInfo.polygonMode                = VK_POLYGON_MODE_FILL;
        configInfo.rasterizationInfo.lineWidth                  = 1.0f;
        configInfo.rasterizationInfo.cullMode                   = VK_CULL_MODE_BACK_BIT;
        configInfo.rasterizationInfo.frontFace                  = VK_FRONT_FACE_CLOCKWISE;
        configInfo.rasterizationInfo.depthBiasEnable            = VK_FALSE;
        configInfo.rasterizationInfo.depthBiasConstantFactor    = 0.0f; // Optional
        configInfo.rasterizationInfo.depthBiasClamp             = 0.0f; // Optional
        configInfo.rasterizationInfo.depthBiasSlopeFactor       = 0.0f; // Optional

        configInfo.multisampleInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        configInfo.multisampleInfo.sampleShadingEnable      = VK_FALSE;
        configInfo.multisampleInfo.rasterizationSamples     = VK_SAMPLE_COUNT_1_BIT;
        configInfo.multisampleInfo.minSampleShading         = 1.0f;     // Optional
        configInfo.multisampleInfo.pSampleMask              = nullptr;  // Optional
        configInfo.multisampleInfo.alphaToCoverageEnable    = VK_FALSE; // Optional
        configInfo.multisampleInfo.alphaToOneEnable         = VK_FALSE; // Optional

        configInfo.colorBlendInfo.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        configInfo.colorBlendInfo.logicOpEnable     = VK_FALSE;
        configInfo.colorBlendInfo.logicOp           = VK_LOGIC_OP_COPY; // Optional
        configInfo.colorBlendInfo.attachmentCount   = static_cast<uint32_t>(configInfo.colorBlendAttachments.size());
        configInfo.colorBlendInfo.pAttachments      = configInfo.colorBlendAttachments.data();
        configInfo.colorBlendInfo.blendConstants[0] = 0.0f;  // Optional
        configInfo.colorBlendInfo.blendConstants[1] = 0.0f;  // Optional
        configInfo.colorBlendInfo.blendConstants[2] = 0.0f;  // Optional
        configInfo.colorBlendInfo.blendConstants[3] = 0.0f;  // Optional

        configInfo.depthStencilInfo.sType                   = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        configInfo.depthStencilInfo.depthTestEnable         = VK_TRUE;
        configInfo.depthStencilInfo.depthWriteEnable        = VK_TRUE;
        configInfo.depthStencilInfo.depthCompareOp          = VK_COMPARE_OP_LESS_OR_EQUAL;
        configInfo.depthStencilInfo.depthBoundsTestEnable   = VK_FALSE;
        configInfo.depthStencilInfo.minDepthBounds          = 0.0f;  // Optional
        configInfo.depthStencilInfo.maxDepthBounds          = 1.0f;  // Optional
        configInfo.depthStencilInfo.stencilTestEnable       = VK_FALSE;
        configInfo.depthStencilInfo.front                   = {};  // Optional
        configInfo.depthStencilInfo.back                    = {};  // Optional

        configInfo.dynamicStateEnables                  = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
        configInfo.dynamicStateInfo.sType               = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        configInfo.dynamicStateInfo.pDynamicStates      = configInfo.dynamicStateEnables.data();
        configInfo.dynamicStateInfo.dynamicStateCount   = static_cast<uint32_t>(configInfo.dynamicStateEnables.size());
        configInfo.dynamicStateInfo.flags               = 0;
        
        configInfo.bindingDescriptions      = ArxModel::Vertex::getBindingDescriptions();
        configInfo.attributeDescriptions    = ArxModel::Vertex::getAttributeDescriptions();
    }

    VkPipelineColorBlendAttachmentState ArxPipeline::createDefaultColorBlendAttachment() {
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask         = 0xf;
        colorBlendAttachment.blendEnable            = VK_FALSE;

        return colorBlendAttachment;
    }

    void ArxPipeline::enableAlphaBlending(PipelineConfigInfo &configInfo) {
        for (size_t i = 0; i < configInfo.colorBlendAttachments.size(); i++) {
            configInfo.colorBlendAttachments[i].blendEnable         = VK_TRUE;
            configInfo.colorBlendAttachments[i].colorWriteMask      = VK_COLOR_COMPONENT_R_BIT |
                                                                      VK_COLOR_COMPONENT_G_BIT |
                                                                      VK_COLOR_COMPONENT_B_BIT |
                                                                      VK_COLOR_COMPONENT_A_BIT;
            configInfo.colorBlendAttachments[i].srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            configInfo.colorBlendAttachments[i].dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            configInfo.colorBlendAttachments[i].colorBlendOp        = VK_BLEND_OP_ADD;
            configInfo.colorBlendAttachments[i].srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            configInfo.colorBlendAttachments[i].dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            configInfo.colorBlendAttachments[i].alphaBlendOp        = VK_BLEND_OP_ADD;
        }
    }

    void ArxPipeline::createComputePipeline(const std::string &compFilepath, VkPipelineLayout& pipelineLayout) {
        auto compCode = readFile(compFilepath);

        createShaderModule(compCode, &computeShaderModule);

        VkPipelineShaderStageCreateInfo compShaderStageInfo{};
        compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compShaderStageInfo.module = computeShaderModule;
        compShaderStageInfo.pName = "main";

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = compShaderStageInfo;
        pipelineInfo.layout = pipelineLayout;

        if (vkCreateComputePipelines(arxDevice.device(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
            throw std::runtime_error("failed to create compute pipeline!");
        }
    }
}
