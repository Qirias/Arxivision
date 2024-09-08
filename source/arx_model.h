#pragma once

#include "../source/arx_device.h"
#include "../source/arx_buffer.h"
#include "../source/managers/arx_buffer_manager.hpp"

namespace arx {
    
    class OcclusionSystem;

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
        uint32_t getInstanceCount() { return instanceCount; }
        
        static uint32_t getWorldWidth() { return worldWidth; }
        static uint32_t getWorldHeight() { return worldHeight; }
        static uint32_t getWorldDepth() { return worldDepth; }
        static uint32_t getTotalInstances() { return totalInstances; }
        
    private:
        void createVertexBuffers(const ArxModel::Builder &builder);
        void createIndexBuffers(const std::vector<uint32_t> &indices);
        void createInstanceBuffer(const ArxModel::Builder &builder);
        static void calculateWorldDimensions(const std::vector<InstanceData> &instanceData);
        
        ArxDevice                   &arxDevice;
        
        std::shared_ptr<ArxBuffer>  vertexBuffer;
        uint32_t                    vertexCount;
        
        std::shared_ptr<ArxBuffer>  instanceBuffer;
        uint32_t                    instanceCount;
        static uint32_t             totalInstances;
        static uint32_t             worldWidth;
        static uint32_t             worldHeight;
        static uint32_t             worldDepth;
        
        
        bool                        hasIndexBuffer = false;
        std::shared_ptr<ArxBuffer>  indexBuffer;
        uint32_t                    indexCount;
    };
}
