#include <stdio.h>
#include "app.h"
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

    App::App() {
        loadGameObjects();
    }

    App::~App() {
        
    }
    
    void App::run() {
        SimpleRenderSystem simpleRenderSystem{arxDevice, arxRenderer.getSwapChainRenderPass()};
        
        while (!arxWindow.shouldClose()) {
            glfwPollEvents();
            
            // beginFrame() will return nullptr if the swapchain need to be recreated
            if (auto commandBuffer = arxRenderer.beginFrame()) {
                arxRenderer.beginSwapChainRenderPass(commandBuffer);
                simpleRenderSystem.renderGameObjects(commandBuffer, gameObjects);
                arxRenderer.endSwapChainRenderPass(commandBuffer);
                arxRenderer.endFrame();
            }
        }
        
        vkDeviceWaitIdle(arxDevice.device());
    }

    void App::loadGameObjects() {
        std::vector<ArxModel::Vertex> vertices {
            {{ 0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
            {{ 0.5f,  0.5f}, {0.0f, 1.0f, 0.0f}},
            {{-0.5f,  0.5f}, {0.0f, 0.0f, 1.0f}}
        };

        auto arxModel = std::make_shared<ArxModel>(arxDevice, vertices);
        
        auto triangle                       = ArxGameObject::createGameObject();
        triangle.model                      = arxModel;
        triangle.color                      = {.1f, .8f, .1f};
        triangle.transform2d.translation.x  = .2f;
        triangle.transform2d.scale          = {2.f, .5f};
        triangle.transform2d.rotation       = .25f * glm::two_pi<float>();
        
        gameObjects.push_back(std::move(triangle));
    }
}
