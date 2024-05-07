#include <stdio.h>
#include "app.h"
#include "systems/simple_render_system.h"
#include "arx_camera.h"
#include "user_input.h"
#include "arx_buffer.h"
#include "chunkManager.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

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

    App::App() : chunkManager{arxDevice}, renderPassManager(arxDevice), textureManager(arxDevice) {
        globalPool = ArxDescriptorPool::Builder(arxDevice)
                    .setMaxSets(ArxSwapChain::MAX_FRAMES_IN_FLIGHT)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ArxSwapChain::MAX_FRAMES_IN_FLIGHT)
                    .build();
    }

    App::~App() {}
    
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
        viewerObject.transform.translation = glm::vec3(0.0, 0.0, -10.0f);
        
        UserInput cameraController{*this};
        
        camera.lookAtRH(viewerObject.transform.translation, viewerObject.transform.translation + cameraController.forwardDir, cameraController.upDir);
        
        float aspect = arxRenderer.getAspectRatio();
        camera.setPerspectiveProjection(glm::radians(60.f), aspect, .1f, 1024.f);
        
        chunkManager.setCamera(camera);
//        chunkManager.obj2vox(gameObjects, "models/house.obj", 0.7f);
//        chunkManager.initializeHeightTerrain(gameObjects, 8);
        chunkManager.initializeTerrain(gameObjects, glm::ivec3(pow(3, 4)));
        
        uint32_t instances = static_cast<uint32_t>(chunkManager.getChunkAABBs().size());
        
        arxRenderer.getSwapChain()->cull.setObjectDataFromAABBs(chunkManager.getChunkAABBs());
        arxRenderer.getSwapChain()->cull.setViewProj(camera.getProjection(), camera.getView(), camera.getInverseView());
        arxRenderer.getSwapChain()->cull.setGlobalData(camera.getProjection(), arxRenderer.getSwapChain()->cull.depthPyramidWidth, arxRenderer.getSwapChain()->cull.depthPyramidHeight, instances);
        arxRenderer.getSwapChain()->loadGeometryToDevice();
        
        std::vector<uint32_t> visibleChunksIndices;// = arxRenderer.getSwapChain()->cull.visibleIndices.indices;
        
        initializeImgui();
        bool freezeCamera = false;
        bool cachedCameraDataIsSet = false;
        
        static OcclusionSystem::GPUCameraData cachedCameraData;
        
        auto currentTime = std::chrono::high_resolution_clock::now();
        while (!arxWindow.shouldClose()) {
            glfwPollEvents();
            
            // Start the ImGui frame
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            
            
            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;
            
            
            ImGui::Begin("Debug");
            ImGui::Text("Chunks %d", static_cast<uint32_t>(visibleChunksIndices.size()));
            ImGui::Checkbox("Frustum Culling", reinterpret_cast<bool*>(&arxRenderer.getSwapChain()->cull.miscData.frustumCulling));
            ImGui::Checkbox("Occlusion Culling", reinterpret_cast<bool*>(&arxRenderer.getSwapChain()->cull.miscData.occlusionCulling));
            ImGui::Checkbox("Freeze Camera", &freezeCamera);
            ImGui::End();

            ImGui::Render();

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
                
                // Cull hidden chunks
                visibleChunksIndices = arxRenderer.getSwapChain()->computeCulling(commandBuffer, instances);

                // Update ubo
                GlobalUbo ubo{};
                ubo.projection      = camera.getProjection();
                ubo.view            = camera.getView();
                ubo.inverseView     = camera.getInverseView();
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                // Early render, objects that were visible last frame
                arxRenderer.beginSwapChainRenderPass(frameInfo, commandBuffer);
                simpleRenderSystem.renderGameObjects(frameInfo, visibleChunksIndices);
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
                arxRenderer.endSwapChainRenderPass(commandBuffer);
                
                // Calculate the depth pyramid
                // Update Dynamic Data for culling if camera is not frozen
                if (freezeCamera) {
                    if (!cachedCameraDataIsSet) {
                        cachedCameraData.view = camera.getView();
                        cachedCameraData.proj = camera.getProjection();
                        cachedCameraData.viewProj = camera.getVP();
                        cachedCameraData.invView = glm::inverse(camera.getView());
                        cachedCameraDataIsSet = true;
                    }
                    arxRenderer.getSwapChain()->cull.cameraBuffer->writeToBuffer(&cachedCameraData);
                }
                else {
                    arxRenderer.getSwapChain()->cull.setViewProj(camera.getProjection(), camera.getView(), camera.getInverseView());
                    arxRenderer.getSwapChain()->updateDynamicData();
                    arxRenderer.getSwapChain()->computeDepthPyramid(commandBuffer);
                    cachedCameraDataIsSet = false;
                }
                
                arxRenderer.endFrame();
                // Use for profiling
//                auto startProgram = std::chrono::high_resolution_clock::now();
//                auto endProgram = std::chrono::high_resolution_clock::now();
//                auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endProgram - startProgram).count();
//                std::cout << "Time: " << duration << " ms" << std::endl;
            }
        }
        vkDeviceWaitIdle(arxDevice.device());
        // Cleanup ImGui when the application is about to close
        vkDestroyDescriptorPool(arxDevice.device(), imguiPool, nullptr);
        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void App::initializeImgui() {
        // The size of the pool is very oversize, but it's copied from imgui demo itself.
        VkDescriptorPoolSize pool_sizes[] =
        {
            { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
            { VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
            { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
            { VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
        };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;

        if (vkCreateDescriptorPool(arxDevice.device(), &pool_info, nullptr, &imguiPool) != VK_SUCCESS) {
            throw std::runtime_error("Failed to create descriptor pool for imgui!");
        }

        // Initializes the core structures of imgui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO(); (void)io;

        // Initializes imgui for GLFW
        ImGui_ImplGlfw_InitForVulkan(arxWindow.getGLFWwindow(), true);

        // Initializes imgui for Vulkan
        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = arxDevice.getInstance();
        init_info.PhysicalDevice = arxDevice.getPhysicalDevice();
        init_info.Device = arxDevice.device();
        init_info.Queue = arxDevice.graphicsQueue();
        init_info.DescriptorPool = imguiPool;
        init_info.MinImageCount = 2;
        init_info.ImageCount = static_cast<uint32_t>(arxRenderer.getSwapChain()->imageCount());
        init_info.MSAASamples = VK_SAMPLE_COUNT_4_BIT;
        init_info.RenderPass = arxRenderer.getSwapChain()->getRenderPass();
        ImGui_ImplVulkan_Init(&init_info);

        ImGui::SetNextWindowSize(ImVec2(400, 300), ImGuiCond_FirstUseEver);
        // You may need to merge fonts or load specific ones
//        io.Fonts->AddFontDefault();
//
//        // Upload Fonts
//        {
//        // Use any command buffer to upload fonts
//        ImGui_ImplVulkan_CreateFontsTexture();
//        ImGui_ImplVulkan_DestroyFontsTexture();
//        }

        // Set clear color for ImGui background
        ImGui::StyleColorsDark();
    }
}
