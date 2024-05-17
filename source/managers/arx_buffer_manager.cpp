#include "arx_buffer_manager.hpp"

namespace arx {

    // Initialize static vectors
    std::vector<std::shared_ptr<ArxBuffer>> BufferManager::vertexBuffers;
    std::vector<VkDeviceSize> BufferManager::vertexOffsets;
    std::vector<std::shared_ptr<ArxBuffer>> BufferManager::indexBuffers;
    std::vector<VkDeviceSize> BufferManager::indexOffsets;
    std::vector<std::shared_ptr<ArxBuffer>> BufferManager::instanceBuffers;
    std::vector<VkDeviceSize> BufferManager::instanceOffsets;
    std::unique_ptr<ArxBuffer> BufferManager::drawIndirectBuffer;

    void BufferManager::addVertexBuffer(std::shared_ptr<ArxBuffer> buffer, VkDeviceSize offset) {
        vertexBuffers.push_back(buffer);
        vertexOffsets.push_back(offset);
    }

    void BufferManager::addIndexBuffer(std::shared_ptr<ArxBuffer> buffer, VkDeviceSize offset) {
        indexBuffers.push_back(buffer);
        indexOffsets.push_back(offset);
    }

    void BufferManager::addInstanceBuffer(std::shared_ptr<ArxBuffer> buffer, VkDeviceSize offset) {
        instanceBuffers.push_back(buffer);
        instanceOffsets.push_back(offset);
    }

    void BufferManager::bindBuffers(VkCommandBuffer commandBuffer) {
        std::vector<VkBuffer> buffers;
        std::vector<VkDeviceSize> offsets;

        // Add vertex buffers and their offsets
        for (size_t i = 0; i < vertexBuffers.size(); ++i) {
            buffers.push_back(vertexBuffers[i]->getBuffer());
            offsets.push_back(vertexOffsets[i]);
        }

        // Add instance buffers and their offsets
        for (size_t i = 0; i < instanceBuffers.size(); ++i) {
            buffers.push_back(instanceBuffers[i]->getBuffer());
            offsets.push_back(instanceOffsets[i]);
        }

        vkCmdBindVertexBuffers(commandBuffer, 0, static_cast<uint32_t>(buffers.size()), buffers.data(), offsets.data());

        // Bind index buffers with their offsets
        for (size_t i = 0; i < indexBuffers.size(); ++i) {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffers[i]->getBuffer(), indexOffsets[i], VK_INDEX_TYPE_UINT32);
        }
    }
}
