#pragma once

#include "arx_device.h"
#include "arx_buffer.h"
#include <vector>
#include <memory>

namespace arx {

    class BufferManager {
    public:
        static void addVertexBuffer(std::shared_ptr<ArxBuffer> buffer, VkDeviceSize offset);
        static void addIndexBuffer(std::shared_ptr<ArxBuffer> buffer, VkDeviceSize offset);
        static void addInstanceBuffer(std::shared_ptr<ArxBuffer> buffer, VkDeviceSize offset);

        static void bindBuffers(VkCommandBuffer commandBuffer);

        static std::unique_ptr<ArxBuffer> drawIndirectBuffer;
        static std::unique_ptr<ArxBuffer> drawCommandCountBuffer;


        static std::vector<std::shared_ptr<ArxBuffer>> vertexBuffers;
        static std::vector<VkDeviceSize> vertexOffsets;
        static std::vector<std::shared_ptr<ArxBuffer>> indexBuffers;
        static std::vector<VkDeviceSize> indexOffsets;
        static std::vector<std::shared_ptr<ArxBuffer>> instanceBuffers;
        static std::vector<VkDeviceSize> instanceOffsets;
    };

}
