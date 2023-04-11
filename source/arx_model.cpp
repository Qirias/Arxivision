#include "arx_model.h"

// std
#include <cassert>
#include <cstring>

namespace arx {

    ArxModel::ArxModel(ArxDevice &device, const ArxModel::Builder &builder)
    : arxDevice{device} {
        createVertexBuffers(builder.vertices);
        createIndexBuffers(builder.indices);
    }

    ArxModel::~ArxModel() {
        vkDestroyBuffer(arxDevice.device(), vertexBuffer, nullptr);
        vkFreeMemory(arxDevice.device(), vertexBufferMemory, nullptr);
        
        if (hasIndexBuffer) {
            vkDestroyBuffer(arxDevice.device(), indexBuffer, nullptr);
            vkFreeMemory(arxDevice.device(), indexBufferMemory, nullptr);
        }
    }
    
    void ArxModel::createVertexBuffers(const std::vector<Vertex> &vertices) {
        vertexCount = static_cast<uint32_t>(vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be at least 3");
        VkDeviceSize bufferSize = sizeof(vertices[0]) * vertexCount;
        
        
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        
        arxDevice.createBuffer(bufferSize,
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               stagingBuffer,
                               stagingBufferMemory);

        void *data;
        vkMapMemory(arxDevice.device(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, vertices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(arxDevice.device(), stagingBufferMemory);
        
        arxDevice.createBuffer(bufferSize,
                               VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                               vertexBuffer,
                               vertexBufferMemory);
        
        arxDevice.copyBuffer(stagingBuffer, vertexBuffer, bufferSize);
        
        vkDestroyBuffer(arxDevice.device(), stagingBuffer, nullptr);
        vkFreeMemory(arxDevice.device(), stagingBufferMemory, nullptr);
    }

    void ArxModel::createIndexBuffers(const std::vector<uint32_t> &indices) {
        indexCount = static_cast<uint32_t>(indices.size());
        hasIndexBuffer = indexCount > 0;
        
        if (!hasIndexBuffer)
            return;
        
        VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
        
        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        
        arxDevice.createBuffer(bufferSize,
                               VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                               VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                               VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                               stagingBuffer,
                               stagingBufferMemory);

        void *data;
        vkMapMemory(arxDevice.device(), stagingBufferMemory, 0, bufferSize, 0, &data);
        memcpy(data, indices.data(), static_cast<size_t>(bufferSize));
        vkUnmapMemory(arxDevice.device(), stagingBufferMemory);
        
        arxDevice.createBuffer(bufferSize,
                               VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                               VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                               indexBuffer,
                               indexBufferMemory);
        
        arxDevice.copyBuffer(stagingBuffer, indexBuffer, bufferSize);
        
        vkDestroyBuffer(arxDevice.device(), stagingBuffer, nullptr);
        vkFreeMemory(arxDevice.device(), stagingBufferMemory, nullptr);
    }

    void ArxModel::draw(VkCommandBuffer commandBuffer) {
        if (hasIndexBuffer) {
            vkCmdDrawIndexed(commandBuffer, indexCount, 1, 0, 0, 0);
        }
        else {
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
    }

    void ArxModel::bind(VkCommandBuffer commandBuffer) {
        VkBuffer buffers[]      = {vertexBuffer};
        VkDeviceSize offsets[]  = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
        
        if (hasIndexBuffer) {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
        }
    }

    std::vector<VkVertexInputBindingDescription> ArxModel::Vertex::getBindingDescriptions() {
        std::vector<VkVertexInputBindingDescription> bindingDescription(1);
        bindingDescription[0].binding   = 0;
        bindingDescription[0].stride    = sizeof(Vertex);
        bindingDescription[0].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
        return bindingDescription;
    }

    std::vector<VkVertexInputAttributeDescription> ArxModel::Vertex::getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions(2);
        attributeDescriptions[0].binding    = 0;
        attributeDescriptions[0].location   = 0;
        attributeDescriptions[0].format     = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset     = offsetof(Vertex, position);
        
        attributeDescriptions[1].binding    = 0;
        attributeDescriptions[1].location   = 1;
        attributeDescriptions[1].format     = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset     = offsetof(Vertex, color);
        return attributeDescriptions;
    }
    
}
