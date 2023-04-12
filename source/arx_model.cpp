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

    std::unique_ptr<ArxModel> ArxModel::createModelFromFile(ArxDevice &device, const std::string &filepath) {
        Builder builder{};
        builder.loadModel(filepath);
        return std::make_unique<ArxModel>(device, builder);
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
        std::vector<VkVertexInputAttributeDescription> attributeDescriptions{};
        
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
