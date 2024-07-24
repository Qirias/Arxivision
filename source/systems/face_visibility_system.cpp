#include "face_visibility_system.hpp"

namespace arx {

    ArxDevice* FaceVisibilitySystem::arxDevice = nullptr;
    std::unique_ptr<ArxDescriptorSetLayout> FaceVisibilitySystem::descriptorSetLayout;
    VkPipelineLayout FaceVisibilitySystem::pipelineLayout;
    std::unique_ptr<ArxPipeline> FaceVisibilitySystem::pipeline;
    std::unique_ptr<ArxDescriptorPool> FaceVisibilitySystem::descriptorPool;
    VkDescriptorSet FaceVisibilitySystem::descriptorSet;
    std::shared_ptr<ArxBuffer> FaceVisibilitySystem::largeInstanceBuffer;
    std::shared_ptr<ArxBuffer> FaceVisibilitySystem::faceVisibilityBuffer;

    void FaceVisibilitySystem::init(ArxDevice &device) {
        arxDevice = &device;
        createDescriptorSetLayout();
        createPipelineLayout();
        createPipeline();
        createDescriptorPool();
        createDescriptorSets();
    }

    void FaceVisibilitySystem::cleanup() {
        vkDestroyPipelineLayout(arxDevice->device(), pipelineLayout, nullptr);
        descriptorSetLayout.reset();
        pipeline.reset();
        descriptorPool.reset();
        largeInstanceBuffer.reset();
        faceVisibilityBuffer.reset();
        arxDevice = nullptr;
    }

    void FaceVisibilitySystem::createDescriptorSetLayout() {
        descriptorSetLayout = ArxDescriptorSetLayout::Builder(*arxDevice)
            .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build();
    }

    void FaceVisibilitySystem::createPipelineLayout() {
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        pushConstantRange.offset = 0;
        pushConstantRange.size = sizeof(WorldDimensionsData);

        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{descriptorSetLayout->getDescriptorSetLayout()};

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

        if (vkCreatePipelineLayout(arxDevice->device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create face visibility pipeline layout!");
        }
    }

    void FaceVisibilitySystem::createPipeline() {
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");

        pipeline = std::make_unique<ArxPipeline>(*arxDevice,
                                                "shaders/faceVisibility.spv",
                                                 pipelineLayout);
    }

    void FaceVisibilitySystem::createDescriptorPool() {
        descriptorPool = ArxDescriptorPool::Builder(*arxDevice)
            .setMaxSets(1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2)
            .build();
    }

    void FaceVisibilitySystem::createDescriptorSets() {
        largeInstanceBuffer = BufferManager::largeInstanceBuffer;
        faceVisibilityBuffer = BufferManager::faceVisibilityBuffer;
        
        auto largeBufferInfo = largeInstanceBuffer->descriptorInfo();
        auto faceVisibilityBufferInfo = faceVisibilityBuffer->descriptorInfo();

        ArxDescriptorWriter(*descriptorSetLayout, *descriptorPool)
            .writeBuffer(0, &largeBufferInfo)
            .writeBuffer(1, &faceVisibilityBufferInfo)
            .build(descriptorSet);
    }

    void FaceVisibilitySystem::removeHiddenFrontFaces(VkCommandBuffer commandBuffer) {
         // Memory barrier between early culling and face visibility compute
         VkMemoryBarrier memoryBarrier = {};
         memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
         memoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
         memoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
         vkCmdPipelineBarrier(
             commandBuffer,
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
             0,
             1, &memoryBarrier,
             0, nullptr,
             0, nullptr
         );
        
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, getPipeline());
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, getPipelineLayout(), 0, 1, &descriptorSet, 0, nullptr);
        
        glm::ivec3 worldSize{ArxModel::getWorldWidth(),
                             ArxModel::getWorldHeight(),
                             ArxModel::getWorldDepth()};
        
        WorldDimensionsData pushConstantData{};
        pushConstantData.worldDimensions = worldSize;
        vkCmdPushConstants(commandBuffer, getPipelineLayout(), VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(WorldDimensionsData), &pushConstantData);

        vkCmdDispatch(commandBuffer, (worldSize.x*CHUNK_SIZE + 7) / 8,
                                     (worldSize.y*CHUNK_SIZE + 7) / 8,
                                     (worldSize.z*CHUNK_SIZE + 7) / 8);
        
         // Memory barrier between face visibility compute and main render pass
         VkMemoryBarrier renderBarrier = {};
         renderBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
         renderBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
         renderBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        
         vkCmdPipelineBarrier(
             commandBuffer,
             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
             VK_PIPELINE_STAGE_VERTEX_SHADER_BIT,
             0,
             1, &renderBarrier,
             0, nullptr,
             0, nullptr
         );
    }
}
