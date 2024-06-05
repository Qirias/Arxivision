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

    // Per-instance data block
    struct InstanceData {
        glm::vec4 translation{};
        glm::vec4 color{};
    };

    struct GPUIndirectDrawCommand {
        VkDrawIndexedIndirectCommand command{};
    };

    class BufferManager {
    public:
        static constexpr int MAX_FRAMES_IN_FLIGHT = 2;
        
        static void addVertexBuffer(std::shared_ptr<ArxBuffer> buffer, VkDeviceSize offset);
        static void addIndexBuffer(std::shared_ptr<ArxBuffer> buffer, VkDeviceSize offset);
        static void addInstanceBuffer(std::shared_ptr<ArxBuffer> buffer, VkDeviceSize offset);

        static void bindBuffers(VkCommandBuffer commandBuffer);
        static void createLargeInstanceBuffer(ArxDevice &device);
        static uint32_t readDrawCommandCount();
        static void resetDrawCommandCountBuffer(VkCommandBuffer commandBuffer);
        
        // occlusion_culling.comp
        static std::unique_ptr<ArxBuffer> drawIndirectBuffer;
        static std::unique_ptr<ArxBuffer> drawCommandCountBuffer;
        static std::unique_ptr<ArxBuffer> instanceOffsetBuffer;
        static std::vector<GPUIndirectDrawCommand> indirectDrawData;
        
        // vertex shader
        static std::shared_ptr<ArxBuffer> largeInstanceBuffer;

        static std::vector<std::shared_ptr<ArxBuffer>> vertexBuffers;
        static std::vector<VkDeviceSize> vertexOffsets;
        static std::vector<std::shared_ptr<ArxBuffer>> indexBuffers;
        static std::vector<VkDeviceSize> indexOffsets;
        static std::vector<std::shared_ptr<ArxBuffer>> instanceBuffers;
        static std::vector<VkDeviceSize> instanceOffsets;
        
    };
}
