//
//  arx_pipeline.h
//  ArXivision
//
//  Created by kiri on 27/3/23.
//
#pragma once
#include <string>
#include <vector>

namespace arx {
    class ArxPipeline {
    public:
        ArxPipeline(const std::string& vertFilepath, const std::string& fragFilepath);
        
    private:
        static std::vector<char> readFile(const std::string& filepath);
        
        void createGraphicsPipeline(const std::string& vertFilepath, const std::string& fragFilepath);
    };
}
