//
//  arx_pipeline.cpp
//  ArXivision
//
//  Created by kiri on 27/3/23.
//

#include <stdio.h>
#include "arx_pipeline.h"
#include <fstream>
#include <stdexcept>
#include <iostream>

namespace arx {
    
    ArxPipeline::ArxPipeline(ArxDevice& device,
                             const std::string& vertFilepath,
                             const std::string& fragFilepath,
                             const PipelineConfigInfo& config) : arxDevice{device} {
        createGraphicsPipeline(vertFilepath, fragFilepath, config);
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

    void ArxPipeline::createGraphicsPipeline(const std::string &vertFilepath, const std::string &fragFilepath, const PipelineConfigInfo& config) {
        
        auto vertCode = readFile(vertFilepath);
        auto fragCode = readFile(fragFilepath);
        
        std::cout << "Vertex Shader code sizes: " << vertCode.size() << std::endl;
        std::cout << "Fragment Shader code sizes: " << fragCode.size() << std::endl;
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

    PipelineConfigInfo ArxPipeline::defaultPipelineConfigInfo(uint32_t width, uint32_t height) {
        PipelineConfigInfo config{};
        
        return config;
    }
}
