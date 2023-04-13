#pragma once

#include "arx_device.h"
#include "arx_buffer.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <vector>
#include <memory>

namespace arx {

    class ArxModel {
    public:
        
        struct Vertex {
            glm::vec3 position{};
            glm::vec3 color{};
            glm::vec3 normal{};
            glm::vec2 uv{};
            
            static std::vector<VkVertexInputBindingDescription> getBindingDescriptions();
            static std::vector<VkVertexInputAttributeDescription> getAttributeDescriptions();
            
            bool operator==(const Vertex &other) const {
                return position == other.position && color == other.color && normal == other.normal && uv == other.uv;
            }
        };
        
        struct Builder {
            std::vector<Vertex> vertices{};
            std::vector<uint32_t> indices{};
            
            void loadModel(const std::string &filepath);
        };
        
        ArxModel(ArxDevice &device, const ArxModel::Builder &builder);
        ~ArxModel();
        
        static std::unique_ptr<ArxModel> createModelFromFile(ArxDevice &device, const std::string &filepath);
        
        ArxModel(const ArxWindow &) = delete;
        ArxModel &operator=(const ArxWindow &) = delete;
        
        void bind(VkCommandBuffer commandBuffer);
        void draw(VkCommandBuffer commandBuffer);
    private:
        void createVertexBuffers(const std::vector<Vertex> &vertices);
        void createIndexBuffers(const std::vector<uint32_t> &indices);

        
        ArxDevice       &arxDevice;
        
        std::unique_ptr<ArxBuffer>  vertexBuffer;
        uint32_t                    vertexCount;
        
        bool                        hasIndexBuffer = false;
        std::unique_ptr<ArxBuffer>  indexBuffer;
        uint32_t                    indexCount;
    };

}
