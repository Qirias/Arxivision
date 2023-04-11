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
        
        struct Builder {
            std::vector<Vertex> vertices{};
            std::vector<uint32_t> indices{};
        };
        
        ArxModel(ArxDevice &device, const ArxModel::Builder &builder);
        ~ArxModel();
        
        ArxModel(const ArxWindow &) = delete;
        ArxModel &operator=(const ArxWindow &) = delete;
        
        void bind(VkCommandBuffer commandBuffer);
        void draw(VkCommandBuffer commandBuffer);
    private:
        void createVertexBuffers(const std::vector<Vertex> &vertices);
        void createIndexBuffers(const std::vector<uint32_t> &indices);

        
        ArxDevice       &arxDevice;
        
        VkBuffer        vertexBuffer;
        VkDeviceMemory  vertexBufferMemory;
        uint32_t        vertexCount;
        
        bool            hasIndexBuffer = false;
        VkBuffer        indexBuffer;
        VkDeviceMemory  indexBufferMemory;
        uint32_t        indexCount;
    };

}
