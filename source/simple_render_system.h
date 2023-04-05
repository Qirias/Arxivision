#pragma once

#include "arx_pipeline.h"
#include "arx_device.h"
#include "arx_game_object.h"

// std
#include <memory>
#include <vector>

namespace arx {

    class SimpleRenderSystem {
    public:
        
        SimpleRenderSystem(ArxDevice &device, VkRenderPass renderPass);
        ~SimpleRenderSystem();
        
        SimpleRenderSystem(const ArxWindow &) = delete;
        SimpleRenderSystem &operator=(const ArxWindow &) = delete;
        
        void renderGameObjects(VkCommandBuffer commandBuffer, std::vector<ArxGameObject> &gameObjects);
    private:
        void createPipelineLayout();
        void createPipeline(VkRenderPass renderPass);
        
        ArxDevice&                      arxDevice;
        
        std::unique_ptr<ArxPipeline>    arxPipeline;
        VkPipelineLayout                pipelineLayout;
    };
}
