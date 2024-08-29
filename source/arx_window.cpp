#include <stdio.h>
#include "../source/arx_window.h"
#include <stdexcept>

namespace arx {
    ArxWindow::ArxWindow(int w, int h, std::string name)
    : width(w), height(h), windowName(name) {
        initWindow();
    }

    ArxWindow::~ArxWindow() {
        glfwDestroyWindow(window);
        glfwTerminate();
    }

    void ArxWindow::initWindow() {
        glfwInit();
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);
        
        window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    }

    void ArxWindow::createWindowSurface(VkInstance instance, VkSurfaceKHR *surface) {
        if (glfwCreateWindowSurface(instance, window, nullptr, surface) != VK_SUCCESS) {
            throw std::runtime_error("failed to create window surface");
        }
    }

    void ArxWindow::framebufferResizeCallback(GLFWwindow *window, int width, int height) {
        auto arxWindow = reinterpret_cast<ArxWindow *>(glfwGetWindowUserPointer(window));
        arxWindow->framebufferResized = true;
        arxWindow->width = width;
        arxWindow->height = height;
    }
}
