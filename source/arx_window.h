//
//  arx_window.h
//  ArXivision
//
//  Created by kiri on 26/3/23.
//
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
        
    private:
        void initWindow();
        
        const int width;
        const int height;
        std::string windowName;
        GLFWwindow *window;
    };
}

