#include "user_input.h"
#include <limits>
#include <iostream>

namespace arx {
    UserInput* UserInput::instance = nullptr;
    double UserInput::xoffset = 0.f;
    double UserInput::yoffset = 0.f;
    double UserInput::lastX = 0.f;
    double UserInput::lastY = 0.f;

    UserInput::UserInput(App& app) : app(app) {
        if (instance == nullptr) {
            instance = this;
        }
        glfwSetCursorPosCallback(app.getWindow().getGLFWwindow(), mouse_callback);
    }

    void UserInput::processInput(GLFWwindow* window, float dt, ArxGameObject& gameObject) {
        
        // Check if rotate is not 0
        if (glm::dot(rotate, rotate) > std::numeric_limits<float>::epsilon()) {
            gameObject.transform.rotation += lookSpeed * glm::normalize(rotate);
        }

        gameObject.transform.rotation.x = glm::clamp(gameObject.transform.rotation.x, -1.5f, 1.5f);
        gameObject.transform.rotation.y = glm::mod(gameObject.transform.rotation.y, glm::two_pi<float>());
        
        glm::vec3 moveDir{0.f};
        if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS) moveDir -= forwardDir;
        if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS) moveDir += forwardDir;
        if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS) moveDir -= rightDir;
        if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS) moveDir += rightDir;
        if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS) moveDir += upDir;
        if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS) moveDir -= upDir;
        
        if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
            gameObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
        }
    }

    void UserInput::updateCameraVectors() {
        rotate.x    = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
        rotate.y    = sin(glm::radians(pitch));
        rotate.z    = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
        forwardDir  = glm::normalize(rotate);
        rightDir    = glm::normalize(glm::cross(forwardDir, worldUp));
        upDir       = glm::normalize(glm::cross(rightDir, forwardDir));
    }

    void UserInput::processMouseMovement() {
        xoffset *= lookSpeed;
        yoffset *= lookSpeed;

        yaw     += xoffset;
        pitch   += yoffset;
        
        updateCameraVectors();
    }

    void UserInput::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
        xoffset = xpos - lastX;
        yoffset = lastY - ypos;
        lastX = xpos;
        lastY = ypos;

        instance->processMouseMovement();
    }
}
