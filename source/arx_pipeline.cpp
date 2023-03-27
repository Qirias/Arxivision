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
    
    ArxPipeline::ArxPipeline(const std::string& vertFilePath, const std::string& fragFilepath) {
        createGraphicsPipeline(vertFilePath, fragFilepath);
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

    void ArxPipeline::createGraphicsPipeline(const std::string &vertFilepath, const std::string &fragFilepath) {
        
        auto vertCode = readFile(vertFilepath);
        auto fragCode = readFile(fragFilepath);
        
        std::cout << "Vertex Shader code sizes: " << vertCode.size() << std::endl;
        std::cout << "Fragment Shader code sizes: " << fragCode.size() << std::endl;
    }
}
