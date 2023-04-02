#include "arx_model.h"

// std
#include <cassert>
#include <cstring>

namespace arx {

    ArxModel::ArxModel(ArxDevice &device, const std::vector<Vertex> &vertices)
    : arxDevice{device} {
        createVertexBuffers(vertices);
    }

    ArxModel::~ArxModel() {
        vkDestroyBuffer(arxDevice.device(), vertexBuffer, nullptr);
        vkFreeMemory(arxDevice.device(), vertexBufferMemory, nullptr);
    }
    
    void ArxModel::createVertexBuffers(const std::vector<Vertex> &vertices) {
        vertexCount = static_cast<uint32_t>(vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be at least 3");
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
        
        arxDevice.createBuffer(bufferSize,
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               vertexBuffer,
                               vertexBufferMemory);
        
        void *data;
        vkMapMemory(arxDevice.device(), vertexBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(arxDevice.device(), vertexBufferMemory);
    }

    void ArxModel::draw(VkCommandBuffer commandBuffer) {
        vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
    }

    void ArxModel::bind(VkCommandBuffer commandBuffer) {
        VkBuffer buffers[]      = {vertexBuffer};
        VkDeviceSize offsets[]  = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
    }

    std::vector<VkVertexInputBindingDescription> ArxModel::Vertex::getBindingDescriptions() {
        std::vector<VkVertexInputBindingDescription> bindingDescription(1);
        bindingDescription[0].binding   = 0;
        bindingDescription[0].stride    = sizeof(Vertex);
        bindingDescription[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    std::vector<VkVertexInputAttributeDescription> ArxModel::Vertex::getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(1);
        attributeDescriptions[0].binding    = 0;
        attributeDescriptions[0].location   = 0;
        attributeDescriptions[0].format     = VK_FORMAT_R32G32_SFLOAT;
        attributeDescriptions[0].offset     = 0;
        return attributeDescriptions;
    }
    
}
