#include "../source/engine_pch.hpp"

#include "../source/app.h"
#include "../source/user_input.h"
#include "../source/arx_buffer.h"
#include "../source/systems/clustered_shading_system.hpp"
#include "../source/geometry/blockMaterials.hpp"
#include "../source/editor/profiling/arx_profiler.hpp"

#define OGT_VOX_IMPLEMENTATION
#include "../libs/ogt_vox.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE

namespace arx {

    App::App() {

        textureManager  = std::make_unique<TextureManager>(arxDevice);
        rpManager       = std::make_unique<RenderPassManager>(arxDevice);
        arxRenderer     = std::make_unique<ArxRenderer>(arxWindow, arxDevice, *rpManager, *textureManager);
        chunkManager    = std::make_unique<ChunkManager>(arxDevice);
        editor          = std::make_shared<Editor>(arxDevice, arxWindow.getGLFWwindow(), *textureManager);

        editor->setRenderPass(arxRenderer->getSwapChain()->getRenderPass());
        editor->init();

        Logger::setEditor(editor);

        Profiler::initializeGPUProfiler(arxDevice, 20);
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

        Logger::shutdown();

        Profiler::cleanup(arxDevice);

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

        auto currentTime = std::chrono::high_resolution_clock::now();
        while (!arxWindow.shouldClose()) {
            glfwPollEvents();

            if (arxRenderer->windowResized()) {
                aspect = arxRenderer->getAspectRatio();
                camera.setPerspectiveProjection(glm::radians(60.f), aspect, .1f, 1024.f);
            }
            
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
                    gameObjects
                };

                Profiler::startFrame(commandBuffer);

                if (!enableCulling) {
                    Profiler::startStageTimer("Occlusion Culling #1", Profiler::Type::GPU, commandBuffer);
                    // Early cull: frustum cull and fill objects that *were* visible last frame
                    BufferManager::resetDrawCommandCountBuffer(frameInfo.commandBuffer);
                    arxRenderer->getSwapChain()->computeCulling(commandBuffer, chunkCount, true);
                    Profiler::stopStageTimer("Occlusion Culling #1", Profiler::Type::GPU, commandBuffer);
                }
                
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
                arxRenderer->Passes(frameInfo, *editor);

                if (!enableCulling) {
                    // Calculate the depth pyramid
                    Profiler::startStageTimer("Depth Pyramid", Profiler::Type::GPU, commandBuffer);
                    arxRenderer->getSwapChain()->cull->setViewProj(camera.getProjection(), camera.getView(), camera.getInverseView());
                    arxRenderer->getSwapChain()->cull->setGlobalData(camera.getProjection(), arxRenderer->getSwapChain()->height(), arxRenderer->getSwapChain()->height(), chunkCount);
                    arxRenderer->getSwapChain()->updateDynamicData();
                    arxRenderer->getSwapChain()->computeDepthPyramid(commandBuffer);
                    Profiler::stopStageTimer("Depth Pyramid", Profiler::Type::GPU, commandBuffer);
                }

                if (!enableCulling) {
                    // Late cull: frustum + occlusion cull and fill objects that were *not* visible last frame
                    Profiler::startStageTimer("Occlusion Culling #2", Profiler::Type::GPU, commandBuffer);
                    arxRenderer->getSwapChain()->computeCulling(commandBuffer, chunkCount);
                    Profiler::stopStageTimer("Occlusion Culling #2", Profiler::Type::GPU, commandBuffer);
                }

                arxRenderer->endFrame();
            }
            
            vkDeviceWaitIdle(arxDevice.device());
        }
    }
}
