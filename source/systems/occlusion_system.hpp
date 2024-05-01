#pragma once

#include "arx_camera.h"
#include "arx_descriptors.h"
#include "arx_pipeline.h"
#include "arx_device.h"
#include "arx_game_object.h"
#include "arx_frame_info.h"

// std
#include <memory>
#include <vector>

namespace arx {

    class OcclusionSystem {
    public:
        
        uint32_t previousPow2(uint32_t v) {
            uint32_t result = 1;
            while (result * 2 < v) result *= 2;
            return result;
        }
        
        struct alignas(16) GPUCameraData {
            glm::mat4 view;
            glm::mat4 proj;
            glm::mat4 viewProj;
            glm::mat4 invView;
        };
        
        struct alignas(16) GPUCullingGlobalData {
            glm::vec4 frustum[6] = { glm::vec4(0) };
            float zNear = 0.1;
            float zFar = 1024.f;
            float P00 = 0;
            float P11 = 0;
            uint32_t pyramidWidth = 0;
            uint32_t pyramidHeight = 0;
            uint32_t totalInstances = 0;
            glm::vec4 _padding;
        };
        
        struct GPUObjectDataBuffer {
            
            struct alignas(16) GPUObjectData {
                glm::vec4 aabbMin;
                glm::vec4 aabbMax;
            };
            
            std::vector<GPUObjectData> data;
            GPUObjectDataBuffer() = default;
            explicit GPUObjectDataBuffer(size_t numObjects) : data(numObjects) {}
            void reset() { std::fill(data.begin(), data.end(), GPUObjectData{}); }
            size_t size() const { return data.size(); }
            GPUObjectData* dataPtr() { return data.data(); }
            size_t bufferSize() const { return data.size() * sizeof(GPUObjectData); }
        };
        
        struct GPUVisibleIndices {
            GPUVisibleIndices() = default;
            std::vector<uint32_t> indices;
            explicit GPUVisibleIndices(size_t numObjects) : indices(numObjects, 0) {}
            void reset() { std::fill(indices.begin(), indices.end(), 0); }
            size_t size() const { return indices.size(); }
            uint32_t* data() { return indices.data(); }
        };
        
        struct GPUDrawVisibility {
            GPUDrawVisibility() = default;
            std::vector<uint32_t> indices;
            explicit GPUDrawVisibility(size_t numObjects) : indices(numObjects, 0) {}
            void reset() { std::fill(indices.begin(), indices.end(), 1); }
            size_t size() const { return indices.size(); }
            uint32_t* data() { return indices.data(); }
        };
        
        OcclusionSystem(ArxDevice &device);
        ~OcclusionSystem();
        
        OcclusionSystem(const OcclusionSystem &) = delete;
        OcclusionSystem &operator=(const OcclusionSystem &) = delete;
        
        void setViewProj(const glm::mat4 &proj, const glm::mat4 &view, const glm::mat4 &inv) {
            cameraData.view = view;
            cameraData.proj = proj;
            cameraData.viewProj = proj * view;
            cameraData.invView  = inv;
        }
        
        void setVisibleIndices(const std::vector<uint32_t> rhs) {
            visibleIndices.reset();
            visibleIndices.indices = rhs;
        }
        
        void setObjectDataFromAABBs(const std::unordered_map<unsigned int, AABB>& chunkAABBs) {
            objectData.data.clear();
            objectData.data.reserve(chunkAABBs.size());
            std::vector<uint32_t> indices;
            indices.reserve(chunkAABBs.size());
            drawVisibility.indices.reserve(chunkAABBs.size());

            for (const auto& c : chunkAABBs) {
                GPUObjectDataBuffer::GPUObjectData gpuObjectData;
                gpuObjectData.aabbMin = glm::vec4(c.second.min, 1.0f);
                gpuObjectData.aabbMax = glm::vec4(c.second.max, 1.0f);
                objectData.data.push_back(gpuObjectData);
                indices.push_back(c.first);
                drawVisibility.indices.push_back(1);
            }
            setVisibleIndices(indices);
        }
        
        glm::vec4 normalizePlane(glm::vec4 p)
        {
            return p / length(glm::vec3(p));
        }
        
        void setGlobalData(const glm::mat4& projectionMatrix, const uint32_t& width, const uint32_t& height, const uint32_t& instances) {
            glm::mat4 projectionT = glm::transpose(projectionMatrix);
            cullingData.frustum[0] = normalizePlane(projectionT[3] + projectionT[0]); // x + w < 0
            cullingData.frustum[1] = normalizePlane(projectionT[3] - projectionT[0]); // x - w > 0
            cullingData.frustum[2] = normalizePlane(projectionT[3] + projectionT[1]); // y + w < 0
            cullingData.frustum[3] = normalizePlane(projectionT[3] - projectionT[1]); // y - w > 0
            cullingData.zNear = 0.1f;
            cullingData.zFar = 1024.f;
            cullingData.P00 = projectionT[0][0];
            cullingData.P11 = projectionT[1][1];
            cullingData.pyramidWidth = width;
            cullingData.pyramidHeight = height;
            cullingData.totalInstances = instances;
        }

    
        // Depth pyramid
        std::unique_ptr<ArxDescriptorPool>          depthDescriptorPool;
        std::unique_ptr<ArxDescriptorSetLayout>     depthDescriptorLayout;
        std::vector<VkDescriptorSet>                depthDescriptorSets;
        VkImage                                     depthPyramidImage;
        VkDeviceMemory                              depthPyramidMemory;
        VkImageView                                 depthPyramidImageView;
        std::vector<VkImageView>                    depthPyramidMips;
        std::vector<VkImageMemoryBarrier>           depthPyramidMipLevelBarriers;
        uint32_t                                    depthPyramidLevels;
        
        uint32_t                                    depthPyramidWidth;
        uint32_t                                    depthPyramidHeight;
        
        VkImageMemoryBarrier                        framebufferDepthWriteBarrier = {};
        VkImageMemoryBarrier                        framebufferDepthReadBarrier = {};
        
        void createDepthPyramidPipelineLayout();
        void createDepthPyramidPipeline();
        
        std::unique_ptr<ArxPipeline>                depthPyramidPipeline;
        VkPipelineLayout                            depthPyramidPipelineLayout;
        
        // Culling stuff
        std::unique_ptr<ArxPipeline>                cullingPipeline;
        VkPipelineLayout                            cullingPipelineLayout;
        std::unique_ptr<ArxDescriptorSetLayout>     cullingDescriptorLayout;
        std::unique_ptr<ArxDescriptorPool>          cullingDescriptorPool;
        VkDescriptorSet                             cullingDescriptorSet;
        
        void createCullingPipelineLayout();
        void createCullingPipeline();
        
        // Late culling stuff, will reuse the same descriptors as the culling. Will only need new pipeline
        std::unique_ptr<ArxPipeline>                lateCullingPipeline;
        VkPipelineLayout                            lateCullingPipelineLayout;
        
        void createLateCullingPipelineLayout();
        void createLateCullingPipeline();
        
        // Buffers for the compute shaders
        std::unique_ptr<ArxBuffer>                  cameraBuffer;
        std::unique_ptr<ArxBuffer>                  objectsDataBuffer;
        std::unique_ptr<ArxBuffer>                  visibilityBuffer;
        std::unique_ptr<ArxBuffer>                  globalDataBuffer;
        std::unique_ptr<ArxBuffer>                  drawVisibilityBuffer;
        
        // Buffers data
        GPUCameraData                               cameraData;
        GPUObjectDataBuffer                         objectData;
        GPUCullingGlobalData                        cullingData;
        GPUVisibleIndices                           visibleIndices;
        GPUDrawVisibility                           drawVisibility;
        
    private:
        ArxDevice&                      arxDevice;
    };
}
