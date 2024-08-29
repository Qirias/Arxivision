#pragma once

#include "../source/arx_pipeline.h"
#include "../source/arx_descriptors.h"

namespace arx {

    class ArxBuffer;
    struct GlobalUbo;

    class ClusteredShading {
    public:
        
        struct alignas(16) Cluster
        {
            glm::vec4 minPoint;
            glm::vec4 maxPoint;
            unsigned int count;
            unsigned int lightIndices[311];
        };
        
        struct Frustum {
            glm::mat4 inverseProjection;
            glm::uvec3 gridSize;
            uint32_t padding0;
            glm::uvec2 screenDimensions;
            float zNear;
            float zFar;
        };
        
        static void init(ArxDevice &device, const int WIDTH, const int HEIGHT);
        static void cleanup();
        
        static void updateUnirofms(GlobalUbo &rhs);
        
        static void dispatchComputeFrustumCluster(VkCommandBuffer commandBuffer);
        static void dispatchComputeClusterCulling(VkCommandBuffer commandBuffer);
        
        static std::shared_ptr<ArxBuffer>               clusterBuffer;
        static std::shared_ptr<ArxBuffer>               frustumParams;
        static std::shared_ptr<ArxBuffer>               pointLightsBuffer;
        static std::shared_ptr<ArxBuffer>               lightCountBuffer;
        static std::shared_ptr<ArxBuffer>               viewMatrixBuffer;
        
        static constexpr unsigned int                   gridSizeX = 16;
        static constexpr unsigned int                   gridSizeY = 9;
        static constexpr unsigned int                   gridSizeZ = 24;
        static constexpr unsigned int                   numClusters = gridSizeX *
                                                                      gridSizeY *
                                                                      gridSizeZ;
        
        static unsigned int                             width;
        static unsigned int                             height;
        
        ClusteredShading() = delete;
        ~ClusteredShading() = delete;
    private:
        static void createDescriptorSetLayout();
        static void createPipelineLayout();
        static void createPipeline();
        static void createDescriptorPool();
        static void createDescriptorSets();
        
        static ArxDevice*                               arxDevice;

        // Frustum Cluster
        static std::unique_ptr<ArxDescriptorSetLayout>  descriptorSetLayoutCluster;
        static VkPipelineLayout                         pipelineLayoutCluster;
        static std::unique_ptr<ArxPipeline>             pipelineCluster;
        static std::unique_ptr<ArxDescriptorPool>       descriptorPoolCluster;
        static VkDescriptorSet                          descriptorSetCluster;
        
        
        // Cluster Culling
        static std::unique_ptr<ArxDescriptorSetLayout>  descriptorSetLayoutCulling;
        static VkPipelineLayout                         pipelineLayoutCulling;
        static std::unique_ptr<ArxPipeline>             pipelineCulling;
        static std::unique_ptr<ArxDescriptorPool>       descriptorPoolCulling;
        static VkDescriptorSet                          descriptorSetCulling;
        
    };
}
