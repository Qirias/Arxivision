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
#include <numeric>

namespace arx {

    App::App() {
        globalPool = ArxDescriptorPool::Builder(arxDevice)
                    .setMaxSets(ArxSwapChain::MAX_FRAMES_IN_FLIGHT)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ArxSwapChain::MAX_FRAMES_IN_FLIGHT)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ArxSwapChain::MAX_FRAMES_IN_FLIGHT)
                    .build();
        createQueryPool();
        initializeImgui();
    }
    App::~App() {}

    void App::run() {
        
        // chunkManager.obj2vox(gameObjects, "models/bunny.obj", 12.f);
        chunkManager.initializeTerrain(gameObjects, glm::ivec3(pow(3, 4)));
        
        std::vector<std::unique_ptr<ArxBuffer>> uboBuffers(ArxSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < uboBuffers.size(); i++) {
            uboBuffers[i] = std::make_unique<ArxBuffer>(arxDevice,
                                                        sizeof(GlobalUbo),
                                                        1,
                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            uboBuffers[i]->map();
        }
        
        // Create large instance buffers that contains all the instance buffers of each chunk
        BufferManager::createLargeInstanceBuffer(arxDevice);

        // Global descriptor set layout
        auto globalSetLayout = ArxDescriptorSetLayout::Builder(arxDevice)
                                .addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_ALL_GRAPHICS)
                                .addBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, VK_SHADER_STAGE_VERTEX_BIT)
                                .build();

        // Create global descriptor sets
        std::vector<VkDescriptorSet> globalDescriptorSets(ArxSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < globalDescriptorSets.size(); i++) {
            VkDescriptorBufferInfo uboBufferInfo = uboBuffers[i]->descriptorInfo();
            VkDescriptorBufferInfo instanceBufferInfo = BufferManager::largeInstanceBuffer->descriptorInfo();

            ArxDescriptorWriter(*globalSetLayout, *globalPool)
                .writeBuffer(0, &uboBufferInfo)
                .writeBuffer(1, &instanceBufferInfo)
                .build(globalDescriptorSets[i]);
        }
        
        uint32_t instances = static_cast<uint32_t>(chunkManager.getChunkAABBs().size());

        // Initialize the maximum indirect draw size
        BufferManager::indirectDrawData.resize(instances);
        
        SimpleRenderSystem simpleRenderSystem{arxDevice, arxRenderer.getSwapChainRenderPass(), globalSetLayout->getDescriptorSetLayout()};
            
//        arxRenderer.init_Passes();

        ArxCamera camera{};

        auto viewerObject = ArxGameObject::createGameObject();
        viewerObject.transform.scale = glm::vec3(0.1);
        viewerObject.transform.translation = glm::vec3(0.0, 0.0, -10.0f);

        UserInput cameraController{*this};

        camera.lookAtRH(viewerObject.transform.translation, viewerObject.transform.translation + cameraController.forwardDir, cameraController.upDir);

        float aspect = arxRenderer.getAspectRatio();
        camera.setPerspectiveProjection(glm::radians(60.f), aspect, .1f, 1024.f);
        chunkManager.setCamera(camera);
        
        // Set data for occlusion culling
        {
            arxRenderer.getSwapChain()->cull.setObjectDataFromAABBs(chunkManager);
            arxRenderer.getSwapChain()->cull.setViewProj(camera.getProjection(), camera.getView(), camera.getInverseView());
            arxRenderer.getSwapChain()->cull.setGlobalData(camera.getProjection(), arxRenderer.getSwapChain()->cull.depthPyramidWidth, arxRenderer.getSwapChain()->cull.depthPyramidHeight, instances);
            arxRenderer.getSwapChain()->loadGeometryToDevice();
        }
        
        bool enableCulling = false;
        
        // Timing variables
        uint64_t cullingTime = 0, renderTime = 0, depthPyramidTime = 0;

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
            ImGui::Text("Press I for ImGui, O for game");
            ImGui::Text("Chunks %d", static_cast<uint32_t>(BufferManager::readDrawCommandCount()));
            ImGui::Checkbox("Frustum Culling", reinterpret_cast<bool*>(&arxRenderer.getSwapChain()->cull.miscData.frustumCulling));
            ImGui::Checkbox("Occlusion Culling", reinterpret_cast<bool*>(&arxRenderer.getSwapChain()->cull.miscData.occlusionCulling));
            ImGui::Checkbox("Freeze Culling", &enableCulling);

            // Display the most recent timings
            ImGui::Text("Culling Time: %.3f ms", cullingTime / 1e6); // Convert ns to ms
            ImGui::Text("Render Time: %.3f ms", renderTime / 1e6);
            ImGui::Text("Depth Pyramid Time: %.3f ms", depthPyramidTime / 1e6);

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

                vkCmdResetQueryPool(commandBuffer, queryPool, 0, 6);
                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 0);

                // Update ubo
                GlobalUbo ubo{};
                ubo.projection      = camera.getProjection();
                ubo.view            = camera.getView();
                ubo.inverseView     = camera.getInverseView();
                ubo.zNear           = .1f;
                ubo.zFar            = 1024.f;
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();
//                arxRenderer.updateMisc(ubo);

                if (!enableCulling) {
                    BufferManager::resetDrawCommandCountBuffer(frameInfo.commandBuffer);
                    
                    // Cull hidden chunks
                    arxRenderer.getSwapChain()->computeCulling(commandBuffer, instances);
                    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 1);
                    
                    VkBufferMemoryBarrier postCullBarrier = {
                       .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                       .pNext = nullptr,
                       .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT,
                       .dstAccessMask = VK_ACCESS_INDIRECT_COMMAND_READ_BIT,
                       .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                       .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                       .buffer = BufferManager::drawCommandCountBuffer->getBuffer(),
                       .offset = 0,
                       .size = sizeof(uint32_t)
                    };

                    vkCmdPipelineBarrier(commandBuffer,
                                         VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                                         VK_PIPELINE_STAGE_DRAW_INDIRECT_BIT,
                                         0,
                                         0, nullptr,
                                         1, &postCullBarrier,
                                         0, nullptr);
                }
                
                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 2);
                // G-Pass
//                arxRenderer.Pass_GBuffer(frameInfo);
                // Early render
                arxRenderer.beginSwapChainRenderPass(frameInfo, commandBuffer);
                simpleRenderSystem.renderGameObjects(frameInfo);
                ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
                arxRenderer.endSwapChainRenderPass(commandBuffer);
                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 3);

                if (!enableCulling) {
                    // Calculate the depth pyramid
                    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 4);
                    
                    arxRenderer.getSwapChain()->cull.setViewProj(camera.getProjection(), camera.getView(), camera.getInverseView());
                    arxRenderer.getSwapChain()->updateDynamicData();
                    arxRenderer.getSwapChain()->computeDepthPyramid(commandBuffer);
                    
                    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 5);
                }

                arxRenderer.endFrame();

                // Retrieve timestamp data
                uint64_t timestamps[6];
                vkGetQueryPoolResults(arxDevice.device(), queryPool, 0, 6, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

                if (!enableCulling) {
                    // Calculate the elapsed time in nanoseconds
                    cullingTime = timestamps[1] - timestamps[0];
                    renderTime = timestamps[3] - timestamps[2];
                    depthPyramidTime = timestamps[5] - timestamps[4];
                } else {
                    renderTime = timestamps[3] - timestamps[2];
                }
//                readDrawCommandCount(BufferManager::drawCommandCountBuffer.get());
            }
        }
        vkDeviceWaitIdle(arxDevice.device());
        // Cleanup ImGui when the application is about to close
        vkDestroyDescriptorPool(arxDevice.device(), imguiPool, nullptr);
        vkDestroyQueryPool(arxDevice.device(), queryPool, nullptr);
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
        ImGui::StyleColorsDark();
    }

    void App::createQueryPool() {
        VkQueryPoolCreateInfo queryPoolInfo = {};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = 6;

        if (vkCreateQueryPool(arxDevice.device(), &queryPoolInfo, nullptr, &queryPool) != VK_SUCCESS) {
            throw std::runtime_error("failed to create query pool!");
        }
    }
}
