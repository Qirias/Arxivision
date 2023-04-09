#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <string>

namespace arx {
    
    class ArxWindow {
    public:
        ArxWindow(int w, int h, std::string name);
        ~ArxWindow();
        
        ArxWindow(const ArxWindow &) = delete;
        ArxWindow &operator=(const ArxWindow &) = delete;
        
        bool shouldClose() { return glfwWindowShouldClose(window); }
        VkExtent2D getExtend() { return {static_cast<uint32_t>(width), static_cast<uint32_t>(height)}; }
        bool wasWindowResized() { return  framebufferReisized; }
        void resetWindowResizedFlag() { framebufferReisized = false; }
        GLFWwindow *getGLFWwindow() const { return window; }
        
        void createWindowSurface(VkInstance instance, VkSurfaceKHR *surface);
        
    private:
        static void framebufferResizeCallback(GLFWwindow *window, int width, int height);
        void initWindow();
        
        int width;
        int height;
        bool framebufferReisized = false;
        
        std::string windowName;
        GLFWwindow *window;
    };
}

