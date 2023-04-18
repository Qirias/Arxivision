#include <stdio.h>
#include "app.h"
#include "systems/simple_render_system.h"
#include "systems/point_light_system.h"
#include "arx_camera.h"
#include "user_input.h"
#include "arx_buffer.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>

// std
#include <stdexcept>
#include <array>
#include <cassert>
#include <chrono>

namespace arx {

    App::App() {
        globalPool = ArxDescriptorPool::Builder(arxDevice)
                    .setMaxSets(ArxSwapChain::MAX_FRAMES_IN_FLIGHT)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ArxSwapChain::MAX_FRAMES_IN_FLIGHT)
                    .build();
        loadGameObjects();
    }

    App::~App() {
        
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
        
        ArxBuffer globalUboBuffer{
            arxDevice,
            sizeof(GlobalUbo),
            ArxSwapChain::MAX_FRAMES_IN_FLIGHT,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT,
            arxDevice.properties.limits.minUniformBufferOffsetAlignment
        };
        
        globalUboBuffer.map();
        
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
        PointLightSystem pointLightSystem{arxDevice, arxRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};

        ArxCamera camera{};
        camera.setViewTarget(glm::vec3(-1.f, -2.f, -2.f), glm::vec3(0.f, 0.f, 2.5f));
        
        auto viewerObject = ArxGameObject::createGameObject();
        viewerObject.transform.translation.z = -2.5f;
        UserInput cameraController{*this};
        
        auto currentTime = std::chrono::high_resolution_clock::now();
        
        while (!arxWindow.shouldClose()) {
            glfwPollEvents();
            
            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;
            
            cameraController.processInput(arxWindow.getGLFWwindow(), frameTime, viewerObject);
            camera.setViewMatrix(viewerObject.transform.translation, cameraController.forwardDir, cameraController.upDir);
            
            float aspect = arxRenderer.getAspectRation();
            camera.setPerspectiveProjection(glm::radians(50.f), aspect, .1f, 100.f);
            
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
                
                // update
                GlobalUbo ubo{};
                ubo.projection      = camera.getProjection();
                ubo.view            = camera.getView();
                ubo.inverseView     = camera.getInverseView();
                pointLightSystem.update(frameInfo, ubo);
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();
                
                // render
                arxRenderer.beginSwapChainRenderPass(commandBuffer);
                
                // order here matters
                simpleRenderSystem.renderGameObjects(frameInfo);
                pointLightSystem.render(frameInfo);
                
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
        
        arxModel = ArxModel::createModelFromFile(arxDevice, "models/quad.obj");
        auto floor                  = ArxGameObject::createGameObject();
        floor.model                 = arxModel;
        floor.transform.translation = {0.f, .5f, 0.f};
        floor.transform.scale       = {3.f, 1.f, 3.f};
        gameObjects.emplace(floor.getId(), std::move(floor));
        
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
