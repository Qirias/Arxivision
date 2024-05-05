#pragma once

#include "arx_device.h"

#include <unordered_map>
#include <string>

namespace arx {

    class RenderPassManager {
    public:
        RenderPassManager(ArxDevice& device);
        ~RenderPassManager();
        
        VkRenderPass getRenderPass(const std::string& name) const;
        VkRenderPass createRenderPass(const std::string& name, const VkRenderPassCreateInfo& createInfo);
        
    private:
        ArxDevice& device;
        std::unordered_map<std::string, VkRenderPass> renderPasses;
        
        void destroyRenderPasses();
    };
}
