#include "../source/engine_pch.hpp"

#include "../source/app.h"
#include "../source/user_input.h"
#include "../source/arx_buffer.h"
#include "../source/systems/clustered_shading_system.hpp"
#include "../source/geometry/blockMaterials.hpp"

#define OGT_VOX_IMPLEMENTATION
#include "../libs/ogt_vox.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

namespace arx {

    App::App() {
        globalPool = ArxDescriptorPool::Builder(arxDevice)
                    .setMaxSets(ArxSwapChain::MAX_FRAMES_IN_FLIGHT)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, ArxSwapChain::MAX_FRAMES_IN_FLIGHT)
                    .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, ArxSwapChain::MAX_FRAMES_IN_FLIGHT)
                    .build();
        
        textureManager  = std::make_unique<TextureManager>(arxDevice);
        rpManager       = std::make_unique<RenderPassManager>(arxDevice);
        arxRenderer     = std::make_unique<ArxRenderer>(arxWindow, arxDevice, *rpManager, *textureManager);
        chunkManager    = std::make_unique<ChunkManager>(arxDevice);
        editor          = std::make_shared<Editor>(arxDevice, arxWindow.getGLFWwindow(), *textureManager);
        editor->setRenderPass(arxRenderer->getSwapChain()->getRenderPass());
        editor->init();

        createQueryPool();
    }

    App::~App() {
        vkDeviceWaitIdle(arxDevice.device());
        
        ClusteredShading::cleanup();
        BufferManager::cleanup();
        
        gameObjects.clear();
        chunkManager.reset();
        rpManager.reset();
        textureManager.reset();
        editor.reset();
        arxRenderer.reset();
        globalPool.reset();

        Logger::shutdown();

        vkDestroyQueryPool(arxDevice.device(), queryPool, nullptr);

        ARX_LOG_INFO("App destructed");
        Logger::shutdown();
    }

    void App::run() {
        ArxCamera camera{};
        UserInput userController{*this};
//        chunkManager->MengerSponge(gameObjects, glm::ivec3(pow(3, 3)));
        if (!chunkManager->vox2Chunks(gameObjects, "data/scenes/monu5Edited.vox"))
            this->~App();
        else
            ARX_LOG_INFO("Initialized scene");
    
    
        // Create large instance buffers that contains all the instance buffers of each chunk that contain the instance data
        // We will use the gl_InstanceIndex in the vertex shader to render from firstInstance + instanceCount
        // Since voxels share the same vertex and index buffer we can bind those once and do multi draw indirect
        BufferManager::createLargeInstanceBuffer(arxDevice, ArxModel::getTotalInstances());
        uint32_t chunkCount = static_cast<uint32_t>(chunkManager->getChunkAABBs().size());
        // Initialize the maximum indirect draw size
        BufferManager::indirectDrawData.resize(chunkCount);
        ARX_LOG_INFO("Total voxels: {}", ArxModel::getTotalInstances());
        
        auto viewerObject = ArxGameObject::createGameObject();
        viewerObject.transform.scale = glm::vec3(0.1);
        viewerObject.transform.translation = glm::vec3(0.0, -20.0, -10.0f);
        
        camera.lookAtRH(viewerObject.transform.translation,
                        viewerObject.transform.translation + userController.forwardDir,
                        userController.upDir);

        float aspect = arxRenderer->getAspectRatio();

        camera.setPerspectiveProjection(glm::radians(60.f), aspect, .1f, 1024.f);
        chunkManager->setCamera(camera);
        
        std::vector<std::unique_ptr<ArxBuffer>> uboBuffers(ArxSwapChain::MAX_FRAMES_IN_FLIGHT);
        for (int i = 0; i < uboBuffers.size(); i++) {
            uboBuffers[i] = std::make_unique<ArxBuffer>(arxDevice,
                                                        sizeof(GlobalUbo),
                                                        1,
                                                        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                                                        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
            uboBuffers[i]->map();
        }

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

        ClusteredShading::init(arxDevice, WIDTH, HEIGHT);
        arxRenderer->init_Passes();

        // Set data for occlusion culling
        {
            arxRenderer->getSwapChain()->cull->setObjectDataFromAABBs(*chunkManager.get());
            arxRenderer->getSwapChain()->cull->setViewProj(camera.getProjection(), camera.getView(), camera.getInverseView());
            arxRenderer->getSwapChain()->cull->setGlobalData(camera.getProjection(), arxRenderer->getSwapChain()->height(), arxRenderer->getSwapChain()->height(), chunkCount);
            arxRenderer->getSwapChain()->loadGeometryToDevice();
        }

        bool enableCulling = false;
        bool ssaoEnabled = true;
        bool ssaoOnly = false;
        bool ssaoBlur = true;
        bool deferred = true;

        // Timing variables
        uint64_t cullingTime = 0, renderTime = 0, depthPyramidTime = 0;

        auto currentTime = std::chrono::high_resolution_clock::now();
        while (!arxWindow.shouldClose()) {
            glfwPollEvents();

            // Start the ImGui frame
            editor->newFrame();
            Editor::EditorImGuiData& imguiData = editor->getImGuiData();

            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            {
                if (!ssaoEnabled) ssaoOnly = false;
                if (ssaoOnly || !ssaoEnabled) ssaoBlur = false;
                if (ssaoOnly) deferred = false;
                // Update the window
                imguiData.cullingTime = cullingTime;
                imguiData.renderTime = renderTime;
                imguiData.depthPyramidTime = depthPyramidTime;
            }

            if (userController.showCartesian()) editor->drawCoordinateVectors(camera);
            editor->drawDebugWindow(imguiData);

            // Fetch from the window
            {
                enableCulling = imguiData.enableCulling;
                ssaoEnabled = imguiData.ssaoEnabled;
                ssaoOnly = imguiData.ssaoOnly;
                ssaoBlur = imguiData.ssaoBlur;
                deferred = imguiData.deferred;
                arxRenderer->getSwapChain()->cull->miscData.frustumCulling = imguiData.frustumCulling;
                arxRenderer->getSwapChain()->cull->miscData.occlusionCulling = imguiData.occlusionCulling;
            }
            
            userController.processInput(arxWindow.getGLFWwindow(), frameTime, viewerObject);
            camera.lookAtRH(viewerObject.transform.translation, viewerObject.transform.translation + userController.forwardDir, userController.upDir);

            // beginFrame() will return nullptr if the swapchain need to be recreated
            if (auto commandBuffer = arxRenderer->beginFrame()) {
                int frameIndex = arxRenderer->getFrameIndex();
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
                if (!enableCulling) {
                    // Early cull: frustum cull and fill objects that *were* visible last frame
                    BufferManager::resetDrawCommandCountBuffer(frameInfo.commandBuffer);
                    arxRenderer->getSwapChain()->computeCulling(commandBuffer, chunkCount, true);
                }
                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 1);
                
                // Update ubo
                GlobalUbo ubo{};
                ubo.projection      = camera.getProjection();
                ubo.view            = camera.getView();
                ubo.inverseView     = camera.getInverseView();
                ubo.zNear           = .1f;
                ubo.zFar            = 1024.f;
                uboBuffers[frameIndex]->writeToBuffer(&ubo);
                uboBuffers[frameIndex]->flush();

                // Update misc for the rest of the render passes
                CompositionParams compParams{};
                compParams.ssao = ssaoEnabled;
                compParams.ssaoOnly = ssaoOnly;
                compParams.ssaoBlur = ssaoBlur;
                compParams.deferred = deferred;
                
                arxRenderer->updateUniforms(ubo, compParams);
                ClusteredShading::updateUniforms(ubo, glm::vec2(arxWindow.getExtend().width, arxWindow.getExtend().height));

                // Passes
                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 2);
                arxRenderer->Passes(frameInfo, *editor);

                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, queryPool, 3);
                if (!enableCulling) {
                    // Calculate the depth pyramid
                    arxRenderer->getSwapChain()->cull->setViewProj(camera.getProjection(), camera.getView(), camera.getInverseView());
                    arxRenderer->getSwapChain()->cull->setGlobalData(camera.getProjection(), arxRenderer->getSwapChain()->height(), arxRenderer->getSwapChain()->height(), chunkCount);
                    arxRenderer->getSwapChain()->updateDynamicData();
                    arxRenderer->getSwapChain()->computeDepthPyramid(commandBuffer);
                }
                vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 4);

                if (!enableCulling) {
                    // Late cull: frustum + occlusion cull and fill objects that were *not* visible last frame
                    arxRenderer->getSwapChain()->computeCulling(commandBuffer, chunkCount);
                    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPool, 5);
                }

                arxRenderer->endFrame();

                // Retrieve timestamp data
                uint64_t timestamps[6];
                vkGetQueryPoolResults(arxDevice.device(), queryPool, 0, 6, sizeof(timestamps), timestamps, sizeof(uint64_t), VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

                if (!enableCulling) {
                    // Calculate the elapsed time in nanoseconds
                    cullingTime = (timestamps[5] - timestamps[4]) + (timestamps[4] - timestamps[3]);
                    renderTime = timestamps[3] - timestamps[2];
                    depthPyramidTime = timestamps[4] - timestamps[3];
                } else {
                    renderTime = timestamps[3] - timestamps[2];
                }
            }
        }
    }

    void App::drawCoordinateVectors(const ArxCamera& camera) {
        int screenWidth = arxRenderer->getSwapChain()->width();
        int screenHeight = arxRenderer->getSwapChain()->height();

        float dpiScale = ImGui::GetIO().DisplayFramebufferScale.x;

        ImVec2 screenCenter = ImVec2((screenWidth * 0.5f) / dpiScale, (screenHeight * 0.5f) / dpiScale);

        ImGui::PushStyleColor(ImGuiCol_WindowBg, IM_COL32(0, 0, 0, 0)); // Transparent background
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0)); // No border

        // Begin an ImGui window with no title bar, resize, move, scrollbar, or collapse functionality
        ImGui::Begin("Coordinate Vectors", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoCollapse);

        ImGui::SetWindowPos(ImVec2(screenCenter.x - (ImGui::GetWindowWidth() * 0.5f), screenCenter.y - (ImGui::GetWindowHeight() * 0.5f)));
        ImGui::SetWindowSize(ImVec2(100 * dpiScale, 100 * dpiScale));

        ImDrawList* draw_list = ImGui::GetWindowDrawList();

        ImVec2 origin = ImVec2(ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x * 0.5f, ImGui::GetCursorScreenPos().y + ImGui::GetContentRegionAvail().y * 0.5f);

        float length = 25.0f * dpiScale;

        ImU32 color_x = IM_COL32(255, 0, 0, 255);
        ImU32 color_y = IM_COL32(0, 255, 0, 255);
        ImU32 color_z = IM_COL32(0, 0, 255, 255);

        glm::mat3 viewRotation = glm::mat3(camera.getView());

        glm::vec3 xAxisWorld(1.0f, 0.0f, 0.0f);
        glm::vec3 yAxisWorld(0.0f, 1.0f, 0.0f);
        glm::vec3 zAxisWorld(0.0f, 0.0f, 1.0f);

        glm::vec3 xAxisView = viewRotation * xAxisWorld;
        glm::vec3 yAxisView = viewRotation * yAxisWorld;
        glm::vec3 zAxisView = viewRotation * zAxisWorld;

        // Project the transformed axes onto the 2D screen
        auto projectAxis = [&](const glm::vec3& axis) -> ImVec2 {
            return ImVec2(
                origin.x + axis.x * length,
                origin.y + axis.y * length
            );
        };

        ImVec2 xEnd = projectAxis(xAxisView);
        ImVec2 yEnd = projectAxis(yAxisView);
        ImVec2 zEnd = projectAxis(zAxisView);

        draw_list->AddLine(origin, xEnd, color_x, 2.0f);
        draw_list->AddText(xEnd, color_x, "X");

        draw_list->AddLine(origin, yEnd, color_y, 2.0f);
        draw_list->AddText(yEnd, color_y, "Y");

        draw_list->AddLine(origin, zEnd, color_z, 2.0f);
        draw_list->AddText(zEnd, color_z, "Z");

        ImGui::End();

        // Pop style colors to revert back to original
        ImGui::PopStyleColor(2);
    }

    void App::createQueryPool() {
        VkQueryPoolCreateInfo queryPoolInfo = {};
        queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
        queryPoolInfo.queryCount = 6;

        if (vkCreateQueryPool(arxDevice.device(), &queryPoolInfo, nullptr, &queryPool) != VK_SUCCESS) {
            ARX_LOG_ERROR("failed to create query pool!");
        }
    }

    void App::printMat4(const glm::mat4& mat) {
        std::cout << "Mat\n";
        std::cout << "[ " << mat[0][0] << " " << mat[0][1] << " " << mat[0][2] << " " << mat[0][3] << " ]" << std::endl;
        std::cout << "[ " << mat[1][0] << " " << mat[1][1] << " " << mat[1][2] << " " << mat[1][3] << " ]" << std::endl;
        std::cout << "[ " << mat[2][0] << " " << mat[2][1] << " " << mat[2][2] << " " << mat[2][3] << " ]" << std::endl;
        std::cout << "[ " << mat[3][0] << " " << mat[3][1] << " " << mat[3][2] << " " << mat[3][3] << " ]" << std::endl;
    }
}
