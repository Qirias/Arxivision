#include "../source/engine_pch.hpp"

#include "../source/managers/arx_buffer_manager.hpp"

namespace arx {

    // Initialize static vectors
    std::vector<std::shared_ptr<ArxBuffer>> BufferManager::vertexBuffers;
    std::vector<VkDeviceSize> BufferManager::vertexOffsets;
    std::vector<std::shared_ptr<ArxBuffer>> BufferManager::indexBuffers;
    std::vector<VkDeviceSize> BufferManager::indexOffsets;
    std::vector<std::shared_ptr<ArxBuffer>> BufferManager::instanceBuffers;
    std::vector<VkDeviceSize> BufferManager::instanceOffsets;

    // Occlusion Culling
    std::shared_ptr<ArxBuffer> BufferManager::largeInstanceBuffer = nullptr;
    std::shared_ptr<ArxBuffer> BufferManager::faceVisibilityBuffer = nullptr;
    std::shared_ptr<ArxBuffer> BufferManager::drawIndirectBuffer = nullptr;
    std::shared_ptr<ArxBuffer> BufferManager::drawCommandCountBuffer = nullptr;
    std::shared_ptr<ArxBuffer> BufferManager::visibilityBuffer = nullptr;
    std::shared_ptr<ArxBuffer> BufferManager::instanceOffsetBuffer = nullptr;

    std::vector<GPUIndirectDrawCommand> BufferManager::indirectDrawData;
    std::vector<uint32_t> BufferManager::visibilityData;
    
    // SVO
    std::shared_ptr<ArxBuffer> BufferManager::nodeBuffer = nullptr;
    std::shared_ptr<ArxBuffer> BufferManager::voxelBuffer = nullptr;

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
        instanceOffsets.push_back(offset/sizeof(InstanceData));
    }

    void BufferManager::createFaceVisibilityBuffer(ArxDevice &device, const uint32_t totalInstances) {
        faceVisibilityBuffer = std::make_shared<ArxBuffer>(
            device,
            sizeof(uint32_t),
            totalInstances,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT |
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        );
        
        faceVisibilityBuffer->map();
        // Initialize the buffer with zeros
        std::vector<uint32_t> initialData(totalInstances, 0);
        faceVisibilityBuffer->writeToBuffer(initialData.data(), initialData.size() * sizeof(uint32_t));
    }

    void BufferManager::createLargeInstanceBuffer(ArxDevice &device, const uint32_t totalInstances) {
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

        stagingBuffer.map();
        VkDeviceSize offset = 0;
        for (size_t j = 0; j < instanceBuffers.size(); ++j) {
            void* data = instanceBuffers[j]->getMappedMemory();
            stagingBuffer.writeToBuffer(data, instanceBuffers[j]->getBufferSize(), offset);
            offset += instanceBuffers[j]->getBufferSize();
        }
        stagingBuffer.unmap();

        device.copyBuffer(stagingBuffer.getBuffer(), largeInstanceBuffer->getBuffer(), totalInstanceBufferSize, 0, 0);
        
        createFaceVisibilityBuffer(device, totalInstances);
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
        
        // Invalidate to fetch updated data in case of non-coherent memory
        drawCommandCountBuffer->invalidate(sizeof(uint32_t));
        drawCount = *static_cast<uint32_t*>(drawCommandCountBuffer->getMappedMemory());
        return drawCount;
    }

    void BufferManager::resetDrawCommandCountBuffer(VkCommandBuffer commandBuffer) {
        // Ensure previous indirect command reads are completed with VK_ACCESS_INDIRECT_COMMAND_READ_BIT
        VkBufferMemoryBarrier prefillBarrier = bufferBarrier(drawCommandCountBuffer->getBuffer(), VK_ACCESS_INDIRECT_COMMAND_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, 0, 1, &prefillBarrier, 0, 0);
    
        vkCmdFillBuffer(commandBuffer,
                        drawCommandCountBuffer->getBuffer(),
                        0,
                        sizeof(uint32_t),
                        0);
        
        // Ensure the buffer operation is complete with VK_ACCESS_TRANSFER_WRITE_BIT
        // Allow subsequent shader stages to read from and write to this buffer
        VkBufferMemoryBarrier fillBarrier = bufferBarrier(drawCommandCountBuffer->getBuffer(), VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT);
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, 0, 1, &fillBarrier, 0, 0);
    }

    VkBufferMemoryBarrier BufferManager::bufferBarrier(VkBuffer buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask) {
        VkBufferMemoryBarrier result = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };

        result.srcAccessMask = srcAccessMask;
        result.dstAccessMask = dstAccessMask;
        result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        result.buffer = buffer;
        result.offset = 0;
        result.size = VK_WHOLE_SIZE;

        return result;
    }

    void BufferManager::createNodeBuffer(ArxDevice &device, const std::vector<GPUNode>& nodes) {
        VkDeviceSize bufferSize = sizeof(GPUNode) * nodes.size();

        ArxBuffer stagingBuffer{
            device,
            bufferSize,
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*)nodes.data());

        nodeBuffer = std::make_shared<ArxBuffer>(
            device,
            bufferSize,
            1,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        device.copyBuffer(stagingBuffer.getBuffer(), nodeBuffer->getBuffer(), bufferSize);
    }

    void BufferManager::createVoxelBuffer(ArxDevice &device, const std::vector<InstanceData>& voxels) {
        VkDeviceSize bufferSize = sizeof(InstanceData) * voxels.size();

        ArxBuffer stagingBuffer{
            device,
            bufferSize,
            1,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };

        stagingBuffer.map();
        stagingBuffer.writeToBuffer((void*)voxels.data());

        voxelBuffer = std::make_shared<ArxBuffer>(
            device,
            bufferSize,
            1,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT |
            VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | // remove SRC when you delete the printVoxelData()
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
        );

        device.copyBuffer(stagingBuffer.getBuffer(), voxelBuffer->getBuffer(), bufferSize);
    }

    void BufferManager::createSVOBuffers(ArxDevice &device, const std::vector<GPUNode>& nodes, const std::vector<InstanceData>& voxels) {
        createNodeBuffer(device, nodes);
        createVoxelBuffer(device, voxels);
   }

    void BufferManager::printVoxelData(ArxDevice& device) {
        if (!voxelBuffer) {
            std::cout << "Voxel buffer is not initialized." << std::endl;
            return;
        }

        VkDeviceSize bufferSize = voxelBuffer->getBufferSize();
        
        // Create a staging buffer
        ArxBuffer stagingBuffer{
            device,
            bufferSize,
            1,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT
        };

        // Copy data from device local buffer to staging buffer
        device.copyBuffer(voxelBuffer->getBuffer(), stagingBuffer.getBuffer(), bufferSize);

        // Map the staging buffer
        void* mappedMemory = nullptr;
        stagingBuffer.map();
        mappedMemory = stagingBuffer.getMappedMemory();

        // Read and print the data
        InstanceData* voxels = reinterpret_cast<InstanceData*>(mappedMemory);
        size_t voxelCount = bufferSize / sizeof(InstanceData);

        std::cout << "Voxel Data:" << std::endl;
        for (size_t i = 0; i < voxelCount; ++i) {
            const auto& voxel = voxels[i];
            std::cout << "Voxel " << i << ":" << std::endl;
            std::cout << "  Translation: (" << voxel.translation.x << ", "
                                            << voxel.translation.y << ", "
                                            << voxel.translation.z << ", "
                                            << voxel.translation.w << ")" << std::endl;
            std::cout << "  Color: (" << voxel.color.x << ", "
                                      << voxel.color.y << ", "
                                      << voxel.color.z << ", "
                                      << voxel.color.w << ")" << std::endl;
            std::cout << "  Visibility Mask: 0x" << std::hex << voxel.visibilityMask << std::dec << std::endl;
            std::cout << std::endl;
        }

        // Unmap the staging buffer
        stagingBuffer.unmap();
    }
}
