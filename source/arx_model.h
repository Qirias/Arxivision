#pragma once

#include "arx_device.h"
#include "arx_buffer.h"
#include "arx_buffer_manager.hpp"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

// std
#include <vector>
#include <memory>

namespace arx {
    
    class OcclusionSystem;

    // Per-instance data block
    struct InstanceData {
        glm::vec3 translation{};
        glm::vec3 color{};
    };

    struct VkDrawIndexedIndirectCommand {
        uint32_t indexCount;
        uint32_t instanceCount;
        uint32_t firstIndex;
        int32_t vertexOffset;
        uint32_t firstInstance;
    };

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
            
            std::vector<InstanceData> instanceData{};
            uint32_t instanceCount{};

            void loadModel(const std::string &filepath);
        };
        
        ArxModel(ArxDevice &device, const ArxModel::Builder &builder);
        ~ArxModel();
        
        static std::unique_ptr<ArxModel> createModelFromFile(ArxDevice &device, const std::string &filepath, uint32_t instanceCount = 1, const std::vector<InstanceData> &data = {});
        
        ArxModel(const ArxModel &) = delete;
        ArxModel &operator=(const ArxModel &) = delete;
        
        void bind(VkCommandBuffer commandBuffer);
        void draw(VkCommandBuffer commandBuffer);
        
        uint32_t getIndexCount() { return indexCount; }

        
    private:
        void createVertexBuffers(const ArxModel::Builder &builder);
        void createIndexBuffers(const std::vector<uint32_t> &indices);
        
        ArxDevice       &arxDevice;
        
        std::shared_ptr<ArxBuffer>  vertexBuffer;
        uint32_t                    vertexCount;
        
        std::shared_ptr<ArxBuffer>  instanceBuffer;
        uint32_t                    instanceCount;
        
        bool                        hasIndexBuffer = false;
        std::shared_ptr<ArxBuffer>  indexBuffer;
        uint32_t                    indexCount;
    };
}
