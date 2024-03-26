#pragma once

#include "arx_camera.h"
#include "arx_pipeline.h"
#include "arx_device.h"
#include "arx_game_object.h"
#include "arx_frame_info.h"
#include "chunkManager.h"

// std
#include <memory>
#include <vector>

namespace arx {

    class SimpleRenderSystem {
    public:
        
        SimpleRenderSystem(ArxDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~SimpleRenderSystem();
        
        SimpleRenderSystem(const SimpleRenderSystem &) = delete;
        SimpleRenderSystem &operator=(const SimpleRenderSystem &) = delete;
        
        void renderGameObjects(FrameInfo &frameInfo, std::vector<Chunk*> &chunks);
    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);
        
        ArxDevice&                      arxDevice;
        
        std::unique_ptr<ArxPipeline>    arxPipeline;
        VkPipelineLayout                pipelineLayout;
    };
}
