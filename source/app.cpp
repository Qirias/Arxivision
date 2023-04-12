#include <stdio.h>
#include "app.h"
#include "simple_render_system.h"
#include "arx_camera.h"
#include "user_input.h"

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
        loadGameObjects();
    }

    App::~App() {
        
    }
    
    void App::run() {
        SimpleRenderSystem simpleRenderSystem{arxDevice, arxRenderer.getSwapChainRenderPass()};
        ArxCamera camera{};
        camera.setViewTarget(glm::vec3(-1.f, -2.f, -2.f), glm::vec3(0.f, 0.f, 2.5f));
        
        auto viewerObject = ArxGameObject::createGameObject();
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
                arxRenderer.beginSwapChainRenderPass(commandBuffer);
                simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects, camera);
                arxRenderer.endSwapChainRenderPass(commandBuffer);
                arxRenderer.endFrame();
            }
        }
        
        vkDeviceWaitIdle(arxDevice.device());
    }

    void App::loadGameObjects() {
        std::shared_ptr<ArxModel> arxModel = ArxModel::createModelFromFile(arxDevice, "models/flat_vase.obj");
        
        auto gameObj = ArxGameObject::createGameObject();
        gameObj.model                  = arxModel;
        gameObj.transform.translation  = {.0f, .5f, 2.5f};
        gameObj.transform.scale        = glm::vec3(2.f, 1.f, 3.f);
        
        gameObjects.push_back(std::move(gameObj));
    }
}
