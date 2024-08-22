#pragma once

#include "arx_pipeline.h"
#include "arx_descriptors.h"
#include "arx_frame_info.h"
#include "blockMaterials.hpp"

namespace arx {

    class ClusteredShading {
    public:
        
        struct alignas(16) Cluster
        {
            glm::vec4 minPoint;
            glm::vec4 maxPoint;
            unsigned int count;
            unsigned int lightIndices[100];
        };
        
        struct frustum {
            glm::mat4 inverseProjection;
            glm::uvec3 gridSize;
            glm::uvec2 screenDimensions;
            float zNear;
            float zFar;
        };
        
        static void init(ArxDevice &device, const int WIDTH, const int HEIGHT);
        static void cleanup();
        
        static void updateMisc(GlobalUbo &rhs);
        
        static void dispatchCompute(VkCommandBuffer commandBuffer);
        
    private:
        ClusteredShading() = delete;
        ~ClusteredShading() = delete;
        
        static void createDescriptorSetLayout();
        static void createPipelineLayout();
        static void createPipeline();
        static void createDescriptorPool();
        static void createDescriptorSets();
        
        static ArxDevice*                               arxDevice;

        static std::unique_ptr<ArxDescriptorSetLayout>  descriptorSetLayout;
        static VkPipelineLayout                         pipelineLayout;
        static std::unique_ptr<ArxPipeline>             pipeline;
        static std::unique_ptr<ArxDescriptorPool>       descriptorPool;
        static VkDescriptorSet                          descriptorSet;
        
        static std::unique_ptr<ArxBuffer>               clusterBuffer;
        static std::unique_ptr<ArxBuffer>               frustumParams;
        
        static constexpr unsigned int                   gridSizeX = 12;
        static constexpr unsigned int                   gridSizeY = 12;
        static constexpr unsigned int                   gridSizeZ = 24;
        static constexpr unsigned int                   numClusters = gridSizeX *
                                                                      gridSizeY *
                                                                      gridSizeZ;
        
        static unsigned int                                      width;
        static unsigned int                                      height;
    };
}
