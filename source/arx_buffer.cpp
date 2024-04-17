/*
 * Encapsulates a vulkan buffer
 *
 * Initially based off VulkanBuffer by Sascha Willems -
 * https://github.com/SaschaWillems/Vulkan/blob/master/base/VulkanBuffer.h
 */

#include "arx_buffer.h"

// std
#include <cassert>
#include <cstring>
#include <iostream>

namespace arx {

    /**
        * Returns the minimum instance size required to be compatible with devices minOffsetAlignment
        *
        * @param instanceSize The size of an instance
        * @param minOffsetAlignment The minimum required alignment, in bytes, for the offset member (eg
        * minUniformBufferOffsetAlignment)
        *
        * @return VkResult of the buffer mapping call
    */
    VkDeviceSize ArxBuffer::getAlignment(VkDeviceSize instanceSize, VkDeviceSize minOffsetAlignment) {
        if (minOffsetAlignment > 0) {
            return (instanceSize + minOffsetAlignment - 1) & ~(minOffsetAlignment - 1);
        }
        return instanceSize;
    }

    ArxBuffer::ArxBuffer(
        ArxDevice &device,
        VkDeviceSize instanceSize,
        uint32_t instanceCount,
        VkBufferUsageFlags usageFlags,
        VkMemoryPropertyFlags memoryPropertyFlags,
    VkDeviceSize minOffsetAlignment)
    : arxDevice{device},
      instanceSize{instanceSize},
      instanceCount{instanceCount},
      usageFlags{usageFlags},
      memoryPropertyFlags{memoryPropertyFlags} {
          alignmentSize = getAlignment(instanceSize, minOffsetAlignment);
          bufferSize = alignmentSize * instanceCount;
          device.createBuffer(bufferSize, usageFlags, memoryPropertyFlags, buffer, memory);
    }

    ArxBuffer::~ArxBuffer() {
        unmap();
        vkDestroyBuffer(arxDevice.device(), buffer, nullptr);
        vkFreeMemory(arxDevice.device(), memory, nullptr);
    }

    /**
        * Map a memory range of this buffer. If successful, mapped points to the specified buffer range.
        *
        * @param size (Optional) Size of the memory range to map. Pass VK_WHOLE_SIZE to map the complete
        * buffer range.
        * @param offset (Optional) Byte offset from beginning
        *
        * @return VkResult of the buffer mapping call
    */
    VkResult ArxBuffer::map(VkDeviceSize size, VkDeviceSize offset) {
        assert(buffer && memory && "Called map on buffer before create");
        return vkMapMemory(arxDevice.device(), memory, offset, size, 0, &mapped);
    }

    /**
        * Unmap a mapped memory range
        *
        * @note Does not return a result as vkUnmapMemory can't fail
    */
    void ArxBuffer::unmap() {
        if (mapped) {
            vkUnmapMemory(arxDevice.device(), memory);
            mapped = nullptr;
        }
    }

    /**
        * Copies the specified data to the mapped buffer. Default value writes whole buffer range
        *
        * @param data Pointer to the data to copy
        * @param size (Optional) Size of the data to copy. Pass VK_WHOLE_SIZE to flush the complete buffer
        * range.
        * @param offset (Optional) Byte offset from beginning of mapped region
        *
    */
    void ArxBuffer::writeToBuffer(void *data, VkDeviceSize size, VkDeviceSize offset) {
        assert(mapped && "Cannot copy to unmapped buffer");

        if (size == VK_WHOLE_SIZE) {
            memcpy(mapped, data, bufferSize);
        } else {
            char *memOffset = (char *)mapped;
            memOffset += offset;
            memcpy(memOffset, data, size);
        }

        if (!(memoryPropertyFlags & VK_MEMORY_PROPERTY_HOST_COHERENT_BIT)) {
            flush(size, offset);
        }
    }

    /**
        * Flush a memory range of the buffer to make it visible to the device
        *
        * @note Only required for non-coherent memory
        *
        * @param size (Optional) Size of the memory range to flush. Pass VK_WHOLE_SIZE to flush the
        * complete buffer range.
        * @param offset (Optional) Byte offset from beginning
        *
        * @return VkResult of the flush call
    */
    VkResult ArxBuffer::flush(VkDeviceSize size, VkDeviceSize offset) {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkFlushMappedMemoryRanges(arxDevice.device(), 1, &mappedRange);
    }

    /**
        * Invalidate a memory range of the buffer to make it visible to the host
        *
        * @note Only required for non-coherent memory
        *
        * @param size (Optional) Size of the memory range to invalidate. Pass VK_WHOLE_SIZE to invalidate
        * the complete buffer range.
        * @param offset (Optional) Byte offset from beginning
        *
        * @return VkResult of the invalidate call
    */
    VkResult ArxBuffer::invalidate(VkDeviceSize size, VkDeviceSize offset) {
        VkMappedMemoryRange mappedRange = {};
        mappedRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
        mappedRange.memory = memory;
        mappedRange.offset = offset;
        mappedRange.size = size;
        return vkInvalidateMappedMemoryRanges(arxDevice.device(), 1, &mappedRange);
    }

    /**
        * Create a buffer info descriptor
        *
        * @param size (Optional) Size of the memory range of the descriptor
        * @param offset (Optional) Byte offset from beginning
        *
        * @return VkDescriptorBufferInfo of specified offset and range
    */
    VkDescriptorBufferInfo ArxBuffer::descriptorInfo(VkDeviceSize size, VkDeviceSize offset) {
        return VkDescriptorBufferInfo{
                buffer,
                offset,
                size,
                };
    }

    /**
        * Copies "instanceSize" bytes of data to the mapped buffer at an offset of index * alignmentSize
        *
        * @param data Pointer to the data to copy
        * @param index Used in offset calculation
        *
    */
    void ArxBuffer::writeToIndex(void *data, int index) {
        writeToBuffer(data, instanceSize, index * alignmentSize);
    }

    /**
        *  Flush the memory range at index * alignmentSize of the buffer to make it visible to the device
        *
        * @param index Used in offset calculation
    *
    */
    VkResult ArxBuffer::flushIndex(int index) { return flush(alignmentSize, index * alignmentSize); }

    /**
        * Create a buffer info descriptor
        *
        * @param index Specifies the region given by index * alignmentSize
        *
        * @return VkDescriptorBufferInfo for instance at index
    */
    VkDescriptorBufferInfo ArxBuffer::descriptorInfoForIndex(int index) {
        return descriptorInfo(alignmentSize, index * alignmentSize);
    }

    /**
        * Invalidate a memory range of the buffer to make it visible to the host
        *
        * @note Only required for non-coherent memory
        *
        * @param index Specifies the region to invalidate: index * alignmentSize
        *
        * @return VkResult of the invalidate call
     */
    VkResult ArxBuffer::invalidateIndex(int index) {
        return invalidate(alignmentSize, index * alignmentSize);
    }
}
