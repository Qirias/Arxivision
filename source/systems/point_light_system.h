#pragma once

#include "arx_camera.h"
#include "arx_pipeline.h"
#include "arx_device.h"
#include "arx_game_object.h"
#include "arx_frame_info.h"

// std
#include <memory>
#include <vector>

namespace arx {

    class PointLightSystem {
    public:
        
        PointLightSystem(ArxDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout);
        ~PointLightSystem();
        
        PointLightSystem(const ArxWindow &) = delete;
        PointLightSystem &operator=(const ArxWindow &) = delete;
        
        void update(FrameInfo &frameInfo, GlobalUbo &ubo);
        void render(FrameInfo &frameInfo);
    private:
        void createPipelineLayout(VkDescriptorSetLayout globalSetLayout);
        void createPipeline(VkRenderPass renderPass);
        
        ArxDevice&                      arxDevice;
        
        std::unique_ptr<ArxPipeline>    arxPipeline;
        VkPipelineLayout                pipelineLayout;
    };
}
