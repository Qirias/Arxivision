#pragma once

#include "../source/managers/arx_texture_manager.hpp"
#include "../source/arx_camera.h"
#include "../source/arx_buffer.h"

#include <../libs/imgui/imgui.h>
#include <../libs/imgui/backends/imgui_impl_glfw.h>
#include <../libs/imgui/backends/imgui_impl_vulkan.h>

namespace arx {

    class Editor {
        public:
        struct EditorImGuiData {
            struct CameraParams {
                uint32_t frustumCulling = true;
                uint32_t occlusionCulling = true;
                uint32_t enableCulling = true;
                uint32_t padding0;
                float zNear = .1f;
                float zFar = 1024.f;
                float speed = 40.f;
                float padding1;
            } camera;

            struct LightingParams {
                uint32_t ssaoEnabled = true;
                uint32_t ssaoOnly = false;
                uint32_t ssaoBlur = true;
                uint32_t deferred = true;
                float directLightColor = 4000;
                float directLightIntensity = .5;
                float perLightMaxDistance = 5.f;
                float padding2;
            } lighting;
        };

        Editor(ArxDevice& device, GLFWwindow* window, TextureManager& textureManager);
        ~Editor();
        void destroyOldRenderPass(); // not sure if I need to do this on windowResize

        void setRenderPass(VkRenderPass swapChainRenderPass) { renderPass = swapChainRenderPass; }
        void init();
        void saveLayout();
        void cleanup();
        void newFrame();
        void render(VkCommandBuffer commandBuffer, VkDescriptorSet descriptorSet);
        void drawDebugWindow();
        void drawCoordinateVectors(ArxCamera& camera);
        void drawConsoleWindow();
        void drawProfilerWindow();

        void addLogMessage(const std::string& message);
        EditorImGuiData& getImGuiData() { return imguiData; }

        static std::shared_ptr<ArxBuffer>   editorDataBuffer;
        static EditorImGuiData              data;

    private:
        ArxDevice&          arxDevice;
        GLFWwindow*         window;
        TextureManager&     textureManager;
        VkRenderPass        renderPass;

        VkDescriptorPool    imguiPool = VK_NULL_HANDLE;

        EditorImGuiData     imguiData;
        
        std::string         iniPath;

        // Console
        std::deque<std::string>     consoleMessages;
        std::mutex                  consoleMutex; // In case of multithreading
        bool                        autoScroll = true;
        int                         maxConsoleMessages = 1000;


        std::deque<std::vector<std::pair<std::string, double>>> profilerDataHistory;
        static const size_t historySize = 30; // Number of frames to average over
        
        ImVec4 getColorForIndex(int index, int total);

        void createImGuiDescriptorPool();
        void createDockSpace();

        static void checkVkResult(VkResult err);
    };
}