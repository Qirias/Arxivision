#pragma once

#include "arx_device.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <vector>

namespace arx {

    class ArxModel {
    public:
        
        struct Vertex {
            glm::vec3 position;
            glm::vec3 color;
            
            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
        };
        
        ArxModel(ArxDevice &device, const std::vector<Vertex> &vertices);
        ~ArxModel();
        
        ArxModel(const ArxWindow &) = delete;
        ArxModel &operator=(const ArxWindow &) = delete;
        
        void bind(VkCommandBuffer commandBuffer);
        void draw(VkCommandBuffer commandBuffer);
    private:
        void createVertexBuffers(const std::vector<Vertex> &vertices);
        
        ArxDevice       &arxDevice;
        VkBuffer        vertexBuffer;
        VkDeviceMemory  vertexBufferMemory;
        uint32_t        vertexCount;
    };

}
