//
//  arx_window.cpp
//  ArXivision
//
//  Created by kiri on 26/3/23.
//

#include <stdio.h>
#include "arx_window.h"

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
        glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);
        
        window = glfwCreateWindow(width, height, windowName.c_str(), nullptr, nullptr);
    }
}
