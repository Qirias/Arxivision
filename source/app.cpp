#include <stdio.h>
#include "app.h"
#include "systems/simple_render_system.h"
#include "systems/point_light_system.h"
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
//        loadGameObjects();
    }

    App::~App() {
        
    }

void printMat4(const glm::mat4& mat) {
    std::cout << "[ " << mat[0][0] << " " << mat[0][1] << " " << mat[0][2] << " " << mat[0][3] << " ]" << std::endl;
    std::cout << "[ " << mat[1][0] << " " << mat[1][1] << " " << mat[1][2] << " " << mat[1][3] << " ]" << std::endl;
    std::cout << "[ " << mat[2][0] << " " << mat[2][1] << " " << mat[2][2] << " " << mat[2][3] << " ]" << std::endl;
    std::cout << "[ " << mat[3][0] << " " << mat[3][1] << " " << mat[3][2] << " " << mat[3][3] << " ]" << std::endl;
}
    
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
//        PointLightSystem pointLightSystem{arxDevice, arxRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};

        ArxCamera camera{};
        camera.setViewTarget(glm::vec3(-1.f, -2.f, -2.f), glm::vec3(0.f, 0.f, 2.5f));
        
       
        
        auto viewerObject = ArxGameObject::createGameObject();
        viewerObject.transform.translation.z = -4.5f;
        viewerObject.transform.translation.y = -8.f;
        UserInput cameraController{*this};
        
        
        camera.setViewMatrix(viewerObject.transform.translation, cameraController.forwardDir, cameraController.upDir);
        
        float aspect = arxRenderer.getAspectRation();
        camera.setPerspectiveProjection(glm::radians(60.f), aspect, .1f, 1024.f);
        
        chunkManager.setCamera(camera);
//        chunkManager.obj2vox(gameObjects, "models/bunny.obj", 4);
        chunkManager.initializeTerrain(gameObjects, glm::ivec3(70));
        
//        chunkManager.obj2vox(gameObjects, "models/star_wars.obj", 15);
    
        auto currentTime = std::chrono::high_resolution_clock::now();
        
        
        while (!arxWindow.shouldClose()) {
            glfwPollEvents();
            
            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;
            
            cameraController.processInput(arxWindow.getGLFWwindow(), frameTime, viewerObject);
            camera.setViewMatrix(viewerObject.transform.translation, cameraController.forwardDir, cameraController.upDir);
                        
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
                
                //chunkManager.Update(gameObjects, camera.getPosition());
                
                // update
                GlobalUbo ubo{};
                ubo.projection      = camera.getProjection();
                ubo.view            = camera.getView();
                ubo.inverseView     = camera.getInverseView();
//                pointLightSystem.update(frameInfo, ubo);
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();
                
                // render
                arxRenderer.beginSwapChainRenderPass(frameInfo, commandBuffer);
                
                // order here matters
                simpleRenderSystem.renderGameObjects(frameInfo, chunkManager);
//                pointLightSystem.render(frameInfo);
                
                arxRenderer.endSwapChainRenderPass(commandBuffer);
                arxRenderer.endFrame();
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
        
        std::vector<glm::vec3> lightColors{
             {1.f, .1f, .1f},
             {.1f, .1f, 1.f},
             {.1f, 1.f, .1f},
             {1.f, 1.f, .1f},
             {.1f, 1.f, 1.f},
             {1.f, 1.f, 1.f}
        };
        
        for (int i = 0; i < lightColors.size(); i++) {
            auto pointLight                     = ArxGameObject::makePointLight(.2f);
            pointLight.color                    = lightColors[i];
            auto rotateLight                    = glm::rotate(glm::mat4(1.f), (i * glm::two_pi<float>()) / lightColors.size(), {0.f, -1.f, 0.f});
            pointLight.transform.translation    = glm::vec3(rotateLight * glm::vec4(-1.f, -1.f, -1.f, 1.f));
            gameObjects.emplace(pointLight.getId(), std::move(pointLight));
        }
    }
}
