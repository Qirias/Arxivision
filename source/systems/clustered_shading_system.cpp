#include "../source/engine_pch.hpp"

#include "../source/systems/clustered_shading_system.hpp"
#include "../source/arx_frame_info.h"
#include "../source/geometry/blockMaterials.hpp"

namespace arx {
    ArxDevice* ClusteredShading::arxDevice = nullptr;

    // Frustum Cluster
    std::unique_ptr<ArxDescriptorSetLayout> ClusteredShading::descriptorSetLayoutCluster;
    VkPipelineLayout ClusteredShading::pipelineLayoutCluster;
    std::unique_ptr<ArxPipeline> ClusteredShading::pipelineCluster;
    std::unique_ptr<ArxDescriptorPool> ClusteredShading::descriptorPoolCluster;
    VkDescriptorSet ClusteredShading::descriptorSetCluster;

    std::shared_ptr<ArxBuffer> ClusteredShading::clusterBuffer;
    std::shared_ptr<ArxBuffer> ClusteredShading::frustumParams;

    // Cluster Culling
    std::unique_ptr<ArxDescriptorSetLayout> ClusteredShading::descriptorSetLayoutCulling;
    VkPipelineLayout ClusteredShading::pipelineLayoutCulling;
    std::unique_ptr<ArxPipeline> ClusteredShading::pipelineCulling;
    std::unique_ptr<ArxDescriptorPool> ClusteredShading::descriptorPoolCulling;
    VkDescriptorSet ClusteredShading::descriptorSetCulling;

    std::shared_ptr<ArxBuffer> ClusteredShading::pointLightsBuffer;
    std::shared_ptr<ArxBuffer> ClusteredShading::lightCountBuffer;
    std::shared_ptr<ArxBuffer> ClusteredShading::viewMatrixBuffer;


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
        vkDestroyPipelineLayout(arxDevice->device(), pipelineLayoutCluster, nullptr);
        descriptorSetLayoutCluster.reset();
        pipelineCluster.reset();
        descriptorPoolCluster.reset();
        
        vkDestroyPipelineLayout(arxDevice->device(), pipelineLayoutCulling, nullptr);
        descriptorSetLayoutCulling.reset();
        pipelineCulling.reset();
        descriptorPoolCulling.reset();
        
        clusterBuffer.reset();
        frustumParams.reset();
        pointLightsBuffer.reset();
        lightCountBuffer.reset();
        viewMatrixBuffer.reset();
    }

    void ClusteredShading::createDescriptorSetLayout() {
        // Frustum Cluster
        descriptorSetLayoutCluster = ArxDescriptorSetLayout::Builder(*arxDevice)
            .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build();
        
        // Cluster Culling
        descriptorSetLayoutCulling = ArxDescriptorSetLayout::Builder(*arxDevice)
            .addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .addBinding(3, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_COMPUTE_BIT)
            .build();
    }

    void ClusteredShading::createPipelineLayout() {
        // Frustum Cluster
        {
            std::vector<VkDescriptorSetLayout> descriptorSetLayouts{descriptorSetLayoutCluster->getDescriptorSetLayout()};
            
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType            = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount   = static_cast<uint32_t>(descriptorSetLayouts.size());
            pipelineLayoutInfo.pSetLayouts      = descriptorSetLayouts.data();
            
            if (vkCreatePipelineLayout(arxDevice->device(), &pipelineLayoutInfo, nullptr, &pipelineLayoutCluster) != VK_SUCCESS) {
                ARX_LOG_ERROR("failed to create frustum cluster pipeline layout!");
            }
        }
        
        // Cluster Culling
        {
            std::vector<VkDescriptorSetLayout> descriptorSetLayouts{descriptorSetLayoutCulling->getDescriptorSetLayout()};
            
            VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
            pipelineLayoutInfo.sType            = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            pipelineLayoutInfo.setLayoutCount   = static_cast<uint32_t>(descriptorSetLayouts.size());
            pipelineLayoutInfo.pSetLayouts      = descriptorSetLayouts.data();
            
            if (vkCreatePipelineLayout(arxDevice->device(), &pipelineLayoutInfo, nullptr, &pipelineLayoutCulling) != VK_SUCCESS) {
                ARX_LOG_ERROR("failed to create cluster culling pipeline layout!");
            }
        }
    }

    void ClusteredShading::createPipeline() {
        // Frustum Cluster
        assert(pipelineLayoutCluster != nullptr && "Cannot create pipeline before pipeline layout");
        
        pipelineCluster = std::make_unique<ArxPipeline>(*arxDevice,
                                                 "shaders/frustum_clusters.spv",
                                                 pipelineLayoutCluster);
        
        // Cluster Culling
        assert(pipelineLayoutCulling != nullptr && "Cannot create pipeline before pipeline layout");
        
        pipelineCulling = std::make_unique<ArxPipeline>(*arxDevice,
                                                 "shaders/cluster_cullLight.spv",
                                                 pipelineLayoutCulling);
    }

    void ClusteredShading::createDescriptorPool() {
        // Frustum Cluster
        descriptorPoolCluster = ArxDescriptorPool::Builder(*arxDevice)
            .setMaxSets(1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1.0f)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1.0f)
            .build();
        
        // Cluster Culling
        descriptorPoolCulling = ArxDescriptorPool::Builder(*arxDevice)
            .setMaxSets(1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 2.0f)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 2.0f)
            .build();
    }

    void ClusteredShading::createDescriptorSets() {
        // Frustum Cluster
        clusterBuffer = std::make_shared<ArxBuffer>(*arxDevice,
                                                    sizeof(Cluster),
                                                    numClusters,
                                                    VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                                                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
        
        frustumParams = std::make_shared<ArxBuffer>(*arxDevice,
                                                    sizeof(Frustum),
                                                    1,
                                                    VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        
        frustumParams->map();
        
        auto clusterBufferInfo = clusterBuffer->descriptorInfo();
        auto frustumParamsInfo = frustumParams->descriptorInfo();
        
        ArxDescriptorWriter(*descriptorSetLayoutCluster, *descriptorPoolCluster)
            .writeBuffer(0, &clusterBufferInfo)
            .writeBuffer(1, &frustumParamsInfo)
            .build(descriptorSetCluster);
        
        // Cluster Culling
        pointLightsBuffer = Materials::pointLightBuffer;
        
        lightCountBuffer = std::make_shared<ArxBuffer>(*arxDevice,
                                                       sizeof(uint32_t),
                                                       1,
                                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        lightCountBuffer->map();
        lightCountBuffer->writeToBuffer(&Materials::currentPointLightCount);
        
        viewMatrixBuffer = std::make_shared<ArxBuffer>(*arxDevice,
                                                       sizeof(glm::mat4),
                                                       1,
                                                       VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                       VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        viewMatrixBuffer->map();
        
        VkDescriptorBufferInfo pointLightBufferInfo{};
        if (Materials::maxPointLights > 0)
            pointLightBufferInfo = pointLightsBuffer->descriptorInfo();
        
        auto lightCountBufferInfo = lightCountBuffer->descriptorInfo();
        auto viewMatrixBufferInfo = viewMatrixBuffer->descriptorInfo();
        
        ArxDescriptorWriter(*descriptorSetLayoutCulling, *descriptorPoolCulling)
            .writeBuffer(0, &clusterBufferInfo)
            .writeBuffer(1, &pointLightBufferInfo)
            .writeBuffer(2, &viewMatrixBufferInfo)
            .writeBuffer(3, &lightCountBufferInfo)
            .build(descriptorSetCulling);
    }

    void ClusteredShading::updateUniforms(GlobalUbo &rhs, glm::vec2 extent) {
        Frustum params{};
        params.inverseProjection = glm::inverse(rhs.projection);
        params.zNear = rhs.zNear;
        params.zFar = rhs.zFar;
        params.gridSize = glm::uvec3(gridSizeX, gridSizeY, gridSizeZ);
        params.screenDimensions = glm::uvec2(extent.x, extent.y);
            
        frustumParams->writeToBuffer(&params);
        
        viewMatrixBuffer->writeToBuffer(&rhs.view);
    }

    void ClusteredShading::dispatchComputeFrustumCluster(VkCommandBuffer commandBuffer) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineCluster->computePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayoutCluster, 0, 1, &descriptorSetCluster, 0, nullptr);

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

    void ClusteredShading::dispatchComputeClusterCulling(VkCommandBuffer commandBuffer) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineCulling->computePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayoutCulling, 0, 1, &descriptorSetCulling, 0, nullptr);

        vkCmdDispatch(commandBuffer, 27, 1, 1);

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
