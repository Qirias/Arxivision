#include <stdio.h>
#include "simple_render_system.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <stdexcept>
#include <array>
#include <cassert>

namespace arx {
    
    struct SimplePushConstantData {
        glm::mat4 modelMatrix{1.f};
        glm::mat4 normalMatrix{1.f};
    };

    SimpleRenderSystem::SimpleRenderSystem(ArxDevice &device, VkRenderPass renderPass, VkDescriptorSetLayout globalSetLayout)
        : arxDevice{device} {
        createPipelineLayout(globalSetLayout);
        createPipeline(renderPass);
    }

    SimpleRenderSystem::~SimpleRenderSystem() {
        vkDestroyPipelineLayout(arxDevice.device(), pipelineLayout, nullptr);
    }
    
    void SimpleRenderSystem::createPipelineLayout(VkDescriptorSetLayout globalSetLayout) {
        
        VkPushConstantRange pushConstantRange{};
        pushConstantRange.stageFlags    = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
        pushConstantRange.offset        = 0;
        pushConstantRange.size          = sizeof(SimplePushConstantData);
        
        std::vector<VkDescriptorSetLayout> descriptorSetLayouts{globalSetLayout};
        
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType                    = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount           = static_cast<uint32_t>(descriptorSetLayouts.size());
        pipelineLayoutInfo.pSetLayouts              = descriptorSetLayouts.data();
        pipelineLayoutInfo.pushConstantRangeCount   = 1;
        pipelineLayoutInfo.pPushConstantRanges      = &pushConstantRange;
        
        if (vkCreatePipelineLayout(arxDevice.device(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
            throw std::runtime_error("failed to create pipeline layout!");
        }
    }

    void SimpleRenderSystem::createPipeline(VkRenderPass renderPass) {
        assert(pipelineLayout != nullptr && "Cannot create pipeline before pipeline layout");
        
        PipelineConfigInfo pipelineConfig{};
        ArxPipeline::defaultPipelineConfigInfo(arxDevice.msaaSamples, pipelineConfig);
        pipelineConfig.renderPass       = renderPass;
        pipelineConfig.pipelineLayout   = pipelineLayout;
        arxPipeline = std::make_unique<ArxPipeline>(arxDevice,
                                                    "shaders/vert.spv",
                                                    "shaders/frag.spv",
                                                    pipelineConfig);
    }

//    void printMat4(const glm::mat4& mat) {
//        std::cout << "[ " << mat[0][0] << " " << mat[0][1] << " " << mat[0][2] << " " << mat[0][3] << " ]" << std::endl;
//        std::cout << "[ " << mat[1][0] << " " << mat[1][1] << " " << mat[1][2] << " " << mat[1][3] << " ]" << std::endl;
//        std::cout << "[ " << mat[2][0] << " " << mat[2][1] << " " << mat[2][2] << " " << mat[2][3] << " ]" << std::endl;
//        std::cout << "[ " << mat[3][0] << " " << mat[3][1] << " " << mat[3][2] << " " << mat[3][3] << " ]" << std::endl;
//    }
    
    void SimpleRenderSystem::renderGameObjects(FrameInfo &frameInfo, std::vector<uint32_t> &visibleChunksIndices) {
        arxPipeline->bind(frameInfo.commandBuffer);
        
        vkCmdBindDescriptorSets(frameInfo.commandBuffer,
                                VK_PIPELINE_BIND_POINT_GRAPHICS,
                                pipelineLayout,
                                0, 1,
                                &frameInfo.globalDescriptorSet,
                                0,
                                nullptr);
        std::cout << "Chunks Rendered: " << visibleChunksIndices.size() << "\n";
        
        for (auto i : visibleChunksIndices) {
            if (frameInfo.voxel[i].model == nullptr) continue;
            SimplePushConstantData push{};
            frameInfo.voxel[i].transform.scale = glm::vec3(VOXEL_SIZE/2);
            push.modelMatrix    = frameInfo.voxel[i].transform.mat4();
            push.normalMatrix   = frameInfo.voxel[i].transform.normalMatrix();


            vkCmdPushConstants(frameInfo.commandBuffer,
                               pipelineLayout,
                               VK_SHADER_STAGE_VERTEX_BIT |
                               VK_SHADER_STAGE_FRAGMENT_BIT,
                               0,
                               sizeof(SimplePushConstantData),
                               &push);
            
            frameInfo.voxel[i].model->bind(frameInfo.commandBuffer);
            frameInfo.voxel[i].model->draw(frameInfo.commandBuffer);
        }
    }
}

/*
 #version 450

 layout (local_size_x = 256) in;

 struct ObjectData {
     vec3 aabbMin;
     vec3 aabbMax;
 };

 layout (set = 0, binding = 0) uniform CameraData {
     mat4 viewProj;
 } camera;

 layout (set = 0, binding = 1) readonly buffer ObjectBuffer {
     ObjectData objects[];
 } objectBuffer;

 layout (set = 0, binding = 2) uniform sampler2D depthPyramid;

 layout (set = 0, binding = 3) buffer VisibilityBuffer {
     uint visibleIndices[];
 } visibilityBuffer;

 layout (set = 0, binding = 4) uniform GlobalData {
     uint pyramidWidth;
     uint pyramidHeight;
     uint instances;
 } globalData;

 layout (set = 0, binding = 5) uniform MiscDynamicData {
     mat4 cullingViewMatrix;
     int occlusionCulling;
 } misc;

 bool isVisibleAABB(vec3 aabbMin, vec3 aabbMax) {
     vec3 boxCenter = 0.5 * (aabbMin + aabbMax);
     vec3 boxHalfExtents = 0.5 * (aabbMax - aabbMin);

     vec4 clipMin = camera.viewProj * vec4(boxCenter - boxHalfExtents, 1.0);
     vec4 clipMax = camera.viewProj * vec4(boxCenter + boxHalfExtents, 1.0);
     
     vec3 clipMinNDC = clipMin.xyz / clipMin.w;
     vec3 clipMaxNDC = clipMax.xyz / clipMax.w;

     vec2 uvMin = clipMinNDC.xy * 0.5 + 0.5;
     vec2 uvMax = clipMaxNDC.xy * 0.5 + 0.5;

     uvMin = clamp(uvMin, vec2(0.0, 0.0), vec2(1.0, 1.0));
     uvMax = clamp(uvMax, vec2(0.0, 0.0), vec2(1.0, 1.0));

     float width = (uvMax.x - uvMin.x) * float(globalData.pyramidWidth);
     float height = (uvMax.y - uvMin.y) * float(globalData.pyramidHeight);
     float lod = max(floor(log2(max(width, height))), 0.0);

     float minDepth = textureLod(depthPyramid, uvMin, lod).r;
     float maxDepth = textureLod(depthPyramid, uvMax, lod).r;

     return (clipMin.z / clipMin.w) <= minDepth || (clipMax.z / clipMax.w) <= maxDepth;
 }

 void main() {
     uint index = gl_GlobalInvocationID.x;
     if (index < globalData.instances) {
         visibilityBuffer.visibleIndices[index] = isVisibleAABB(objectBuffer.objects[index].aabbMin, objectBuffer.objects[index].aabbMax) ? 1 : 0;
     }
 }

 */
