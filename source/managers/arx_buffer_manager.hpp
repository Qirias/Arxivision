#pragma once

#include "arx_device.h"
#include "arx_buffer.h"

#include <vector>
#include <memory>
#include <unordered_map>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>

namespace arx {

    // SVO Nodes
    struct GPUNode {
        glm::vec3 min;
        uint32_t childrenStartIndex;
        glm::vec3 max;
        uint32_t voxelStartIndex;
    };

    // Per-instance data block
    struct InstanceData {
        glm::vec4 translation{};
        glm::vec4 color{};
        uint32_t visibilityMask{0x3F};
        uint32_t padding[3];
    };

    struct GPUIndirectDrawCommand {
        VkDrawIndexedIndirectCommand command{};
        uint32_t drawID;
        uint32_t padding[2];
    };

    class BufferManager {
    public:
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        
        static void addVertexBuffer(std::shared_ptr<ArxBuffer> buffer, VkDeviceSize offset);
        static void addIndexBuffer(std::shared_ptr<ArxBuffer> buffer, VkDeviceSize offset);
        static void addInstanceBuffer(std::shared_ptr<ArxBuffer> buffer, VkDeviceSize offset);

        static void bindBuffers(VkCommandBuffer commandBuffer);
        static void createLargeInstanceBuffer(ArxDevice &device, const uint32_t totalInstances);
        static void createFaceVisibilityBuffer(ArxDevice &device, const uint32_t totalInstances);
        static uint32_t readDrawCommandCount();
        static void resetDrawCommandCountBuffer(VkCommandBuffer commandBuffer);
        static VkBufferMemoryBarrier bufferBarrier(VkBuffer buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
        
        // occlusion_culling.comp
        static std::unique_ptr<ArxBuffer> drawIndirectBuffer;
        static std::unique_ptr<ArxBuffer> drawCommandCountBuffer;
        static std::unique_ptr<ArxBuffer> instanceOffsetBuffer;
        static std::unique_ptr<ArxBuffer> visibilityBuffer;
        static std::vector<GPUIndirectDrawCommand> indirectDrawData;
        static std::vector<uint32_t> visibilityData;
        
        // vertex shader
        static std::shared_ptr<ArxBuffer> largeInstanceBuffer;
        
        // face visibility
        static std::shared_ptr<ArxBuffer> faceVisibilityBuffer;
        
        // SVO
        static void createSVOBuffers(ArxDevice &device, 
                                     const std::vector<GPUNode>& nodes,
                                     const std::vector<InstanceData>& voxels);
        
        static std::shared_ptr<ArxBuffer> nodeBuffer;
        static std::shared_ptr<ArxBuffer> voxelBuffer;

        static std::vector<std::shared_ptr<ArxBuffer>> vertexBuffers;
        static std::vector<VkDeviceSize> vertexOffsets;
        static std::vector<std::shared_ptr<ArxBuffer>> indexBuffers;
        static std::vector<VkDeviceSize> indexOffsets;
        static std::vector<std::shared_ptr<ArxBuffer>> instanceBuffers;
        static std::vector<VkDeviceSize> instanceOffsets;
        
    private:
        // SVO
        static void createNodeBuffer(ArxDevice &device, const std::vector<GPUNode>& nodes);
        static void createVoxelBuffer(ArxDevice &device, const std::vector<InstanceData>& voxels);
    };
}
