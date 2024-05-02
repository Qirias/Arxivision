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
        // Toggle ImGui interaction based on key presses
        if (glfwGetKey(window, GLFW_KEY_I) == GLFW_PRESS) {
            enableImGuiInteraction();
        } else if (glfwGetKey(window, GLFW_KEY_O) == GLFW_PRESS) {
            disableImGuiInteraction();
        }

        // Only process camera and movement controls if ImGui is not active
        if (!isImGuiActive) {
            // Movement handling
            glm::vec3 moveDir{0.f};
            if (glfwGetKey(window, keys.moveForward) == GLFW_PRESS)    moveDir -= forwardDir;
            if (glfwGetKey(window, keys.moveBackward) == GLFW_PRESS)   moveDir += forwardDir;
            if (glfwGetKey(window, keys.moveLeft) == GLFW_PRESS)       moveDir -= rightDir;
            if (glfwGetKey(window, keys.moveRight) == GLFW_PRESS)      moveDir += rightDir;
            if (glfwGetKey(window, keys.moveUp) == GLFW_PRESS)         moveDir += upDir;
            if (glfwGetKey(window, keys.moveDown) == GLFW_PRESS)       moveDir -= upDir;

            if (glm::dot(moveDir, moveDir) > std::numeric_limits<float>::epsilon()) {
                gameObject.transform.translation += moveSpeed * dt * glm::normalize(moveDir);
            }
            processMouseMovement();
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
        if (!isImGuiActive) {
            xoffset *= lookSpeed;
            yoffset *= lookSpeed;

            yaw     += xoffset;
            pitch   += yoffset;

            updateCameraVectors();
        }
    }

    void UserInput::mouse_callback(GLFWwindow* window, double xpos, double ypos) {
        if (instance->isImGuiActive) {
            // Update ImGui mouse position if ImGui is active
            ImGui::GetIO().MousePos = ImVec2((float)xpos, (float)ypos);
        } else {
            // Process game/application mouse movement
            xoffset = xpos - lastX;
            yoffset = lastY - ypos;
            lastX = xpos;
            lastY = ypos;

            instance->processMouseMovement();
        }
    }

    void UserInput::enableImGuiInteraction() {
        isImGuiActive = true;
        glfwSetInputMode(app.getWindow().getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        ImGui::GetIO().MouseDrawCursor = true;
    }

    void UserInput::disableImGuiInteraction() {
        isImGuiActive = false;
        glfwSetInputMode(app.getWindow().getGLFWwindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        ImGui::GetIO().MouseDrawCursor = false;
    }
}
