#include "arx_model.h"

#include "arx_utils.h"

// libs
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

// std
#include <cassert>
#include <cstring>
#include <unordered_map>
#include <iostream>

namespace std {
    template<>
    struct hash<arx::ArxModel::Vertex> {
        size_t operator()(arx::ArxModel::Vertex const &vertex) const {
            size_t seed = 0;
            arx::hashCombine(seed, vertex.position, vertex.color, vertex.normal, vertex.uv);
            return seed;
        }
    };
}

namespace arx {

    uint32_t ArxModel::totalInstances = 0;

    ArxModel::ArxModel(ArxDevice &device, const ArxModel::Builder &builder)
    : arxDevice{device} {
        createVertexBuffers(builder);
        createIndexBuffers(builder.indices);
        createInstanceBuffer(builder);
        
        totalInstances += instanceCount;
    }

    ArxModel::~ArxModel() {}

    std::unique_ptr<ArxModel> ArxModel::createModelFromFile(ArxDevice &device, const std::string &filepath, uint32_t instanceCount, const std::vector<InstanceData> &data) {
        
        Builder builder{};
        builder.instanceCount = instanceCount;
        builder.loadModel(filepath);
        builder.instanceData = data;
       
        return std::make_unique<ArxModel>(device, builder);
    }

    void ArxModel::createVertexBuffers(const ArxModel::Builder &builder) {
        vertexCount = static_cast<uint32_t>(builder.vertices.size());
        assert(vertexCount >= 3 && "Vertex count must be at least 3");
        VkDeviceSize bufferSize = sizeof(builder.vertices[0]) * vertexCount;
        uint32_t vertexSize = sizeof(builder.vertices[0]);
        
        auto stagingBuffer = std::make_shared<ArxBuffer>(
                   arxDevice,
                   vertexSize,
                   vertexCount,
                   VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                   VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
               );
        
        stagingBuffer->map();
        stagingBuffer->writeToBuffer((void *)builder.vertices.data());
        
        
        vertexBuffer = std::make_shared<ArxBuffer>(
                                                   arxDevice,
                                                   vertexSize,
                                                   vertexCount,
                                                   VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
                                                   VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                                   VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        arxDevice.copyBuffer(stagingBuffer->getBuffer(), vertexBuffer->getBuffer(), bufferSize);
        
        // Calculate and register offset
        VkDeviceSize vertexBufferOffset = 0;
        for (const auto& buffer : BufferManager::vertexBuffers) {
           vertexBufferOffset += buffer->getBufferSize();
            
        }
        BufferManager::addVertexBuffer(stagingBuffer, vertexBufferOffset);
    }

    void ArxModel::createIndexBuffers(const std::vector<uint32_t> &indices) {
        indexCount = static_cast<uint32_t>(indices.size());
        hasIndexBuffer = indexCount > 0;

        if (!hasIndexBuffer)
            return;

        VkDeviceSize bufferSize = sizeof(indices[0]) * indexCount;
        uint32_t indexSize = sizeof(indices[0]);

        auto stagingBuffer = std::make_shared<ArxBuffer>(
                 arxDevice,
                 indexSize,
                 indexCount,
                 VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                 VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
             );

        stagingBuffer->map();
        stagingBuffer->writeToBuffer((void *)indices.data());

        indexBuffer = std::make_shared<ArxBuffer>(
            arxDevice,
            indexSize,
            indexCount,
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        arxDevice.copyBuffer(stagingBuffer->getBuffer(), indexBuffer->getBuffer(), bufferSize);

        // Calculate and register offset
        VkDeviceSize indexBufferOffset = 0;
        for (const auto& buffer : BufferManager::indexBuffers) {
            indexBufferOffset += buffer->getBufferSize();
        }
        BufferManager::addIndexBuffer(stagingBuffer, indexBufferOffset);
    }


    void ArxModel::createInstanceBuffer(const ArxModel::Builder &builder) {
        VkDeviceSize instanceBufferSize = builder.instanceCount * sizeof(InstanceData);
        
        uint64_t instanceSize = sizeof(InstanceData);
        instanceCount = builder.instanceCount;

        auto instanceStagingBuffer = std::make_shared<ArxBuffer>(
            arxDevice,
            instanceSize,
            instanceCount,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );

        instanceStagingBuffer->map();
        instanceStagingBuffer->writeToBuffer((void*)builder.instanceData.data());

        instanceBuffer = std::make_shared<ArxBuffer>(
            arxDevice,
            instanceSize,
            instanceCount,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        arxDevice.copyBuffer(instanceStagingBuffer->getBuffer(), instanceBuffer->getBuffer(), instanceBufferSize);

        // Calculate and register offset
        uint32_t instanceBufferOffset = 0;
        for (const auto& buffer : BufferManager::instanceBuffers) {
            instanceBufferOffset += buffer->getBufferSize();
        }
            
        BufferManager::addInstanceBuffer(instanceStagingBuffer, instanceBufferOffset);
    }

    // Not using it anymore, used it for instanced rendering
    void ArxModel::draw(VkCommandBuffer commandBuffer) {
        if (hasIndexBuffer) {
            vkCmdDrawIndexed(commandBuffer, indexCount, instanceCount, 0, 0, 0);
        }
        else {
            vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);
        }
    }
    
    void ArxModel::bind(VkCommandBuffer commandBuffer) {
        VkBuffer buffers[]      = {vertexBuffer->getBuffer()};
        VkDeviceSize offsets[]  = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, buffers, offsets);
        
        if (hasIndexBuffer) {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffer->getBuffer(), 0, VK_INDEX_TYPE_UINT32);
        }
    }

    std::vector<VkVertexInputBindingDescription> ArxModel::Vertex::getBindingDescriptions() {
        return {
           {0, sizeof(Vertex), VK_VERTEX_INPUT_RATE_VERTEX}
       };
    }

    std::vector<VkVertexInputAttributeDescription> ArxModel::Vertex::getAttributeDescriptions() {
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};

        // Binding 0: Vertex attributes
        attributeDescriptions.push_back({0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, position)});
        attributeDescriptions.push_back({1, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, color)});
        attributeDescriptions.push_back({2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(Vertex, normal)});
        attributeDescriptions.push_back({3, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(Vertex, uv)});

        return attributeDescriptions;
    }

    void ArxModel::Builder::loadModel(const std::string &filepath) {
        tinyobj::attrib_t attrib;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warn, err;
        
        if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, filepath.c_str())) {
            throw std::runtime_error(warn + err);
        }
        
        vertices.clear();
        indices.clear();
        
        std::unordered_map<Vertex, uint32_t> uniqueVertices{};
        for (const auto &shape : shapes) {
            for (const auto &index : shape.mesh.indices) {
                Vertex vertex{};
                
                if (index.vertex_index >= 0) {
                    vertex.position = {
                        attrib.vertices[3 * index.vertex_index + 0],
                        attrib.vertices[3 * index.vertex_index + 1],
                        attrib.vertices[3 * index.vertex_index + 2],
                    };
                    
                    
                    vertex.color = {
                        attrib.colors[3 * index.vertex_index + 0],
                        attrib.colors[3 * index.vertex_index + 1],
                        attrib.colors[3 * index.vertex_index + 2]
                    };
                }
                
                if (index.normal_index >= 0) {
                    vertex.normal = {
                        attrib.normals[3 * index.normal_index + 0],
                        attrib.normals[3 * index.normal_index + 1],
                        attrib.normals[3 * index.normal_index + 2],
                    };
                }
                
                if (index.texcoord_index >= 0) {
                    vertex.uv = {
                        attrib.texcoords[2 * index.texcoord_index + 0],
                        attrib.texcoords[2 * index.texcoord_index + 1]
                    };
                }
                
                if (uniqueVertices.count(vertex) == 0) {
                    uniqueVertices[vertex] = static_cast<uint32_t>(vertices.size());
                    vertices.push_back(vertex);
                }
                indices.push_back(uniqueVertices[vertex]);
            }
        }
    }
}
