#include <stdio.h>
#include "app.h"
#include "systems/simple_render_system.h"
#include "arx_camera.h"
#include "user_input.h"
#include "arx_buffer.h"
#include "chunkManager.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <stdexcept>
#include <array>
#include <cassert>
#include <chrono>
#include <iostream>

namespace arx {

    App::App() : chunkManager{arxDevice} {
        globalPool = ArxDescriptorPool::Builder(arxDevice)
                    .setMaxSets(ArxSwapChain::MAX_FRAMES_IN_FLIGHT)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ArxSwapChain::MAX_FRAMES_IN_FLIGHT)
                    .build();
    }

    App::~App() {
        
    }

//    void printMat4(const glm::mat4& mat) {
//        std::cout << "Mat\n";
//        std::cout << "[ " << mat[0][0] << " " << mat[0][1] << " " << mat[0][2] << " " << mat[0][3] << " ]" << std::endl;
//        std::cout << "[ " << mat[1][0] << " " << mat[1][1] << " " << mat[1][2] << " " << mat[1][3] << " ]" << std::endl;
//        std::cout << "[ " << mat[2][0] << " " << mat[2][1] << " " << mat[2][2] << " " << mat[2][3] << " ]" << std::endl;
//        std::cout << "[ " << mat[3][0] << " " << mat[3][1] << " " << mat[3][2] << " " << mat[3][3] << " ]" << std::endl;
//    }
    
    void App::run() {
        
        std::vector<std::unique_ptr<ArxBuffer>> uboBuffers(ArxSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < uboBuffers.size(); i++) {
            uboBuffers[i] = std::make_unique<ArxBuffer>(arxDevice,
                                                        sizeof(GlobalUbo),
                                                        1,
                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            uboBuffers[i]->map();
        }
        
        auto globalSetLayout = ArxDescriptorSetLayout::Builder(arxDevice)
                            .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                            .build();

        std::vector<VkDescriptorSet> globalDescriptorSets(ArxSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < globalDescriptorSets.size(); i++) {
            auto bufferInfo = uboBuffers[i]->descriptorInfo();
            ArxDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &bufferInfo)
                .build(globalDescriptorSets[i]);
        }
        
        SimpleRenderSystem simpleRenderSystem{arxDevice, arxRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};

        ArxCamera camera{};
        
        auto viewerObject = ArxGameObject::createGameObject();
        viewerObject.transform.scale = glm::vec3(0.1);
        UserInput cameraController{*this};
        
        
        camera.lookAtRH(viewerObject.transform.translation, viewerObject.transform.translation + cameraController.forwardDir, cameraController.upDir);
        
        float aspect = arxRenderer.getAspectRation();
        camera.setPerspectiveProjection(glm::radians(60.f), aspect, .1f, 1024.f);
        
        chunkManager.setCamera(camera);
//        chunkManager.obj2vox(gameObjects, "models/bunny.obj", 15);
//        chunkManager.initializeHeightTerrain(gameObjects, 8);
        chunkManager.initializeTerrain(gameObjects, glm::ivec3(pow(3, 4)));
        
        uint32_t instances = static_cast<uint32_t>(chunkManager.getChunkAABBs().size());
        
        arxRenderer.getSwapChain()->cull.setObjectDataFromAABBs(chunkManager.getChunkAABBs());
        arxRenderer.getSwapChain()->cull.setViewProj(camera.getProjection(), camera.getView(), camera.getInverseView());
        arxRenderer.getSwapChain()->cull.setGlobalData(camera.getProjection(), arxRenderer.getSwapChain()->cull.depthPyramidWidth, arxRenderer.getSwapChain()->cull.depthPyramidHeight, instances);
        arxRenderer.getSwapChain()->loadGeometryToDevice();
        
        auto currentTime = std::chrono::high_resolution_clock::now();
        while (!arxWindow.shouldClose()) {
            glfwPollEvents();
            
            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;
            
            cameraController.processInput(arxWindow.getGLFWwindow(), frameTime, viewerObject);
            camera.lookAtRH(viewerObject.transform.translation, viewerObject.transform.translation + cameraController.forwardDir, cameraController.upDir);

            // beginFrame() will return nullptr if the swapchain need to be recreated
            if (auto commandBuffer = arxRenderer.beginFrame()) {
                int frameIndex = arxRenderer.getFrameIndex();
                FrameInfo frameInfo{
                    frameIndex,
                    frameTime,
                    commandBuffer,
                    camera,
                    globalDescriptorSets[frameIndex],
                    gameObjects
                };
                
                
                // Frustum culling
//                camera.setPerspectiveProjection(glm::radians(40.f), aspect, .1f, 1024.f);
//                camera.cull_chunks_against_frustum(chunkManager.GetChunkPositions(), visibleChunksIndices, CHUNK_SIZE);
                
                // Update Dynamic Data
                arxRenderer.getSwapChain()->cull.setViewProj(camera.getProjection(), camera.getView(), camera.getInverseView());
                arxRenderer.getSwapChain()->updateDynamicData();

                std::vector<uint32_t> visibleChunksIndices;
                visibleChunksIndices = arxRenderer.getSwapChain()->computeCulling(commandBuffer, instances);
                
//                camera.setPerspectiveProjection(glm::radians(60.f), aspect, .1f, 1024.f);

                // update
                GlobalUbo ubo{};
                ubo.projection      = camera.getProjection();
                ubo.view            = camera.getView();
                ubo.inverseView     = camera.getInverseView();
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();
                
                // render
                arxRenderer.beginSwapChainRenderPass(frameInfo, commandBuffer);
                
                // order here matters
                simpleRenderSystem.renderGameObjects(frameInfo, visibleChunksIndices);
                
                arxRenderer.endSwapChainRenderPass(commandBuffer);
                arxRenderer.getSwapChain()->computeDepthPyramid(commandBuffer);
                arxRenderer.endFrame();
                visibleChunksIndices.clear();
            }
        }
        vkDeviceWaitIdle(arxDevice.device());
    }

    void App::loadGameObjects() {
        std::shared_ptr<ArxModel> arxModel = ArxModel::createModelFromFile(arxDevice, "models/smooth_vase.obj");
        
        auto gameObj                    = ArxGameObject::createGameObject();
        gameObj.model                   = arxModel;
        gameObj.transform.translation   = {.0f, .5f, 0.f};
        gameObj.transform.scale         = glm::vec3(3.f);
        gameObjects.emplace(gameObj.getId(), std::move(gameObj));
        
        arxModel = ArxModel::createModelFromFile(arxDevice, "models/cube.obj");
        
        for (int i = 0; i < 64; i++) {
            for (int j = 0; j < 64; j++) {
                auto floor = ArxGameObject::createGameObject();
                floor.model                   = arxModel;
                floor.transform.translation   = {i * .2f - 6, .7f, j * .2f - 6};
                floor.transform.scale         = glm::vec3(.1f);
                gameObjects.emplace(floor.getId(), std::move(floor));
            }
        }
    }
}
