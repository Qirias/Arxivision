#include "arx_buffer_manager.hpp"
#include <iostream>

namespace arx {

    // Initialize static vectors
    std::vector<std::shared_ptr<ArxBuffer>> BufferManager::vertexBuffers;
    std::vector<VkDeviceSize> BufferManager::vertexOffsets;
    std::vector<std::shared_ptr<ArxBuffer>> BufferManager::indexBuffers;
    std::vector<VkDeviceSize> BufferManager::indexOffsets;
    std::vector<std::shared_ptr<ArxBuffer>> BufferManager::instanceBuffers;
    std::vector<VkDeviceSize> BufferManager::instanceOffsets;

    std::shared_ptr<ArxBuffer> BufferManager::largeInstanceBuffer = nullptr;
    std::unique_ptr<ArxBuffer> BufferManager::drawIndirectBuffer = nullptr;
    std::unique_ptr<ArxBuffer> BufferManager::drawCommandCountBuffer = nullptr;
    std::unique_ptr<ArxBuffer> BufferManager::instanceOffsetBuffer = nullptr;

    std::vector<GPUIndirectDrawCommand> BufferManager::indirectDrawData;

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

    void BufferManager::createLargeInstanceBuffer(ArxDevice &device) {
        // Calculate the total size of the instance buffer once
        VkDeviceSize totalInstanceBufferSize = 0;
        for (const auto &buffer : instanceBuffers) {
            totalInstanceBufferSize += buffer->getBufferSize();
        }

        // Create the large instance buffer (device-local, not host-visible)
        largeInstanceBuffer = std::make_shared<ArxBuffer>(
            device,
            sizeof(InstanceData),
            totalInstanceBufferSize / sizeof(InstanceData),
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        // Create a staging buffer (host-visible)
        ArxBuffer stagingBuffer{
            device,
            sizeof(InstanceData),
            static_cast<uint32_t>(totalInstanceBufferSize / sizeof(InstanceData)),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };

        // Map the staging buffer
        stagingBuffer.map();
        VkDeviceSize offset = 0;
        for (size_t j = 0; j < instanceBuffers.size(); ++j) {
            void* data = instanceBuffers[j]->getMappedMemory();
            stagingBuffer.writeToBuffer(data, instanceBuffers[j]->getBufferSize(), offset);
            offset += instanceBuffers[j]->getBufferSize();
        }
        stagingBuffer.unmap();

        // Copy data from the staging buffer to the large instance buffer
        device.copyBuffer(stagingBuffer.getBuffer(), largeInstanceBuffer->getBuffer(), totalInstanceBufferSize, 0, 0);
    }

    void BufferManager::bindBuffers(VkCommandBuffer commandBuffer) {
        std::vector<VkBuffer> buffers;
        std::vector<VkDeviceSize> offsets;
        
        // Add vertex buffers and their offsets
        for (size_t i = 0; i < vertexBuffers.size(); ++i) {
            buffers.push_back(vertexBuffers[i]->getBuffer());
            offsets.push_back(vertexOffsets[i]);
        }
        
        vkCmdBindVertexBuffers(commandBuffer, 0, static_cast<uint32_t>(buffers.size()), buffers.data(), offsets.data());
        
        // Bind index buffers with their offsets
        for (size_t i = 0; i < indexBuffers.size(); ++i) {
            vkCmdBindIndexBuffer(commandBuffer, indexBuffers[i]->getBuffer(), indexOffsets[i], VK_INDEX_TYPE_UINT32);
        }
    }

    uint32_t BufferManager::readDrawCommandCount() {
        uint32_t drawCount = 0;
        if (!drawCommandCountBuffer->getMappedMemory()) {
            drawCommandCountBuffer->map(sizeof(uint32_t));
        }
        drawCommandCountBuffer->invalidate(sizeof(uint32_t));
        drawCount = *static_cast<uint32_t*>(drawCommandCountBuffer->getMappedMemory());
        return drawCount;
    }

    void BufferManager::resetDrawCommandCountBuffer(VkCommandBuffer commandBuffer) {
        vkCmdFillBuffer(commandBuffer,
                        BufferManager::drawCommandCountBuffer->getBuffer(),
                        0,
                        sizeof(uint32_t),
                        0);
        
        VkBufferMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT;
        barrier.buffer = BufferManager::drawCommandCountBuffer->getBuffer();
        barrier.offset = 0;
        barrier.size = sizeof(uint32_t);
        vkCmdPipelineBarrier(commandBuffer,
                             VK_PIPELINE_STAGE_TRANSFER_BIT,
                             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             0,
                             0, nullptr,
                             1, &barrier,
                             0, nullptr);
    }
}
