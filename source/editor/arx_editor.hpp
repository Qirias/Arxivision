#pragma once

// #include "../source/arx_device.h"
#include "../source/managers/arx_texture_manager.hpp"
#include "../source/arx_camera.h"

#include <../libs/imgui/imgui.h>
#include <../libs/imgui/backends/imgui_impl_glfw.h>
#include <../libs/imgui/backends/imgui_impl_vulkan.h>

namespace arx {

    class Editor {
        public:
            struct EditorImGuiData {
            bool frustumCulling = true;
            bool occlusionCulling = true;
            bool enableCulling = false;
            bool ssaoEnabled = true;
            bool ssaoOnly = false;
            bool ssaoBlur = true;
            bool deferred = true;
            uint64_t cullingTime;
            uint64_t renderTime;
            uint64_t depthPyramidTime;
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
        void drawDebugWindow(EditorImGuiData& data);
        void drawCoordinateVectors(ArxCamera& camera);
        void drawConsoleWindow();

        void addLogMessage(const std::string& message);
        EditorImGuiData& getImGuiData() { return imguiData; }

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
        std::mutex                  consoleMutex;
        bool                        autoScroll = true;
        int                         maxConsoleMessages = 1000;

        void createImGuiDescriptorPool();
        void createDockSpace();

        static void checkVkResult(VkResult err);
    };
}