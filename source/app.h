//
//  app.h
//  ArXivision
//
//  Created by kiri on 26/3/23.
//
#pragma once

#include "arx_window.h"
#include "arx_pipeline.h"
#include "arx_device.h"
#include "arx_swap_chain.h"
#include "arx_model.h"

// std
#include <memory>
#include <vector>

namespace arx {

    class App {
    public:
        static constexpr int WIDTH  = 800;
        static constexpr int HEIGHT = 600;
        
        App();
        ~App();
        
        App(const ArxWindow &) = delete;
        App &operator=(const ArxWindow &) = delete;
        
        void run();
        
    private:
        void loadModels();
        void createPipelineLayout();
        void createPipeline();
        void createCommandBuffers();
        void freeCommandBuffers();
        void drawFrame();
        void recreateSwapChain();
        void recordCommandBuffer(int imageIndex);
        
        ArxWindow                       arxWindow{WIDTH, HEIGHT, "Hello Vulkan!"};
        ArxDevice                       arxDevice{arxWindow};
        std::unique_ptr<ArxSwapChain>   arxSwapChain;
        std::unique_ptr<ArxPipeline>    arxPipeline;
        VkPipelineLayout                pipelineLayout;
        std::vector<VkCommandBuffer>    commandBuffers;
        std::unique_ptr<ArxModel>       arxModel;
    };
}
