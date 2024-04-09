#pragma once

#include "arx_camera.h"
#include "arx_descriptors.h"
#include "arx_pipeline.h"
#include "arx_device.h"
#include "arx_game_object.h"
#include "arx_frame_info.h"
#include "chunkManager.h"

// std
#include <memory>
#include <vector>

namespace arx {

    class OcclusionSystem {
    public:
        
        uint32_t previousPow2(uint32_t v) {
            uint32_t result = 1;
            while (result * 2 < v) result *= 2;
            return result;
        }
        
        OcclusionSystem(ArxDevice &device);
        ~OcclusionSystem();
        
        OcclusionSystem(const OcclusionSystem &) = delete;
        OcclusionSystem &operator=(const OcclusionSystem &) = delete;
        
        std::unique_ptr<ArxDescriptorPool>          depthDescriptorPool;
        std::unique_ptr<ArxDescriptorSetLayout>     depthDescriptorLayout;
        std::vector<VkDescriptorSet>                depthDescriptorSets;
        VkImage                                     depthPyramidImage;
        VkDeviceMemory                              depthPyramidMemory;
        VkImageView                                 depthPyramidImageView;
        std::vector<VkImageView>                    depthPyramidMips;
        std::vector<VkImageMemoryBarrier>           depthPyramidMipLevelBarriers;
        uint32_t                                    depthPyramidLevels;
        
        uint32_t                                    depthPyramidWidth;
        uint32_t                                    depthPyramidHeight;
        
        VkImageMemoryBarrier                        framebufferDepthWriteBarrier = {};
        VkImageMemoryBarrier                        framebufferDepthReadBarrier = {};
        
        void createPipelineLayout();
        void createPipeline();
                
        std::unique_ptr<ArxPipeline>    arxPipeline;
        VkPipelineLayout                pipelineLayout;
        
    private:
        ArxDevice&                      arxDevice;
    };
}
