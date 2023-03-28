//
//  arx_pipeline.h
//  ArXivision
//
//  Created by kiri on 27/3/23.
//
#pragma once

#include "arx_device.h"

#include <string>
#include <vector>

namespace arx {

    struct PipelineConfigInfo {};

    class ArxPipeline {
    public:
        ArxPipeline(ArxDevice& device,
                    const std::string& vertFilepath,
                    const std::string& fragFilepath,
                    const PipelineConfigInfo& config);
        
        ~ArxPipeline() {}
        
        ArxPipeline(const ArxPipeline&) = delete;
        void operator=(const ArxPipeline&) = delete;
        
        static PipelineConfigInfo defaultPipelineConfigInfo(uint32_t width, uint32_t height);
        
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
