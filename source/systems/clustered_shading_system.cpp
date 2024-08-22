#include "clustered_shading_system.hpp"

namespace arx {
    ArxDevice* ClusteredShading::arxDevice = nullptr;
    std::unique_ptr<ArxDescriptorSetLayout> ClusteredShading::descriptorSetLayout;
    VkPipelineLayout ClusteredShading::pipelineLayout;
    std::unique_ptr<ArxPipeline> ClusteredShading::pipeline;
    std::unique_ptr<ArxDescriptorPool> ClusteredShading::descriptorPool;
    VkDescriptorSet ClusteredShading::descriptorSet;

    std::unique_ptr<ArxBuffer> ClusteredShading::clusterBuffer;
    std::unique_ptr<ArxBuffer> ClusteredShading::frustumParams;


    unsigned int ClusteredShading::width;
    unsigned int ClusteredShading::height;

    constexpr unsigned int ClusteredShading::gridSizeX;
    constexpr unsigned int ClusteredShading::gridSizeY;
    constexpr unsigned int ClusteredShading::gridSizeZ;
    constexpr unsigned int ClusteredShading::numClusters;

    void ClusteredShading::init(ArxDevice &device, const int WIDTH, const int HEIGHT) {
        arxDevice = &device;
        width = WIDTH;
        height = HEIGHT;
        
        createDescriptorSetLayout();
        createPipelineLayout();
        createPipeline();
        createDescriptorPool();
        createDescriptorSets();
    }

    void ClusteredShading::cleanup() {
        vkDestroyPipelineLayout(arxDevice->device(), pipelineLayout, nullptr);
        descriptorSetLayout.reset();
        pipeline.reset();
        descriptorPool.reset();
        arxDevice = nullptr;
    }

    void ClusteredShading::createDescriptorSetLayout() {
        descriptorSetLayout = ArxDescriptorSetLayout::Builder(*arxDevice)
            .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build();
    }
    

    void ClusteredShading::createPipelineLayout() {
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{descriptorSetLayout->getDescriptorSetLayout()};
        
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType            = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount   = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts      = descriptorSetLayouts.data();
        
        if (vkCreatePipelineLayout(arxDevice->device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create tiled shading pipeline layout!");
        }
    }

    void ClusteredShading::createPipeline() {
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");
        
        pipeline = std::make_unique<ArxPipeline>(*arxDevice,
                                                 "shaders/frustum_clusters.spv",
                                                 pipelineLayout);
    }

    void ClusteredShading::createDescriptorPool() {
        descriptorPool = ArxDescriptorPool::Builder(*arxDevice)
            .setMaxSets(1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1.0f)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f)
            .build();
    }

    void ClusteredShading::createDescriptorSets() {
        clusterBuffer = std::make_unique<ArxBuffer>(*arxDevice,
                                                    sizeof(Cluster),
                                                    numClusters,
                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        frustumParams = std::make_unique<ArxBuffer>(*arxDevice,
                                                    sizeof(frustum),
                                                    1,
                                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        frustumParams->map();
        
        auto clusterBufferInfo = clusterBuffer->descriptorInfo();
        auto frustumParamsInfo = frustumParams->descriptorInfo();
        
        ArxDescriptorWriter(*descriptorSetLayout, *descriptorPool)
            .writeBuffer(0, &clusterBufferInfo)
            .writeBuffer(1, &frustumParamsInfo)
            .build(descriptorSet);
    }

    void ClusteredShading::updateMisc(GlobalUbo &rhs) {
        frustum params{};
        params.inverseProjection = glm::inverse(rhs.projection);
        params.zNear = rhs.zNear;
        params.zFar = rhs.zFar;
        params.gridSize = {gridSizeX, gridSizeY, gridSizeZ};
        params.screenDimensions = {width, height};
        
        frustumParams->writeToBuffer(&params);
    }

    void ClusteredShading::dispatchCompute(VkCommandBuffer commandBuffer) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline->computePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);

        vkCmdDispatch(commandBuffer, gridSizeX, gridSizeY, gridSizeZ);

        VkMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(commandBuffer,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                            0,
                            1, &barrier,
                            0, nullptr,
                            0, nullptr);
    }
}
