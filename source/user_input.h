#pragma once

#include "arx_game_object.h"
#include "app.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

namespace arx {
    class UserInput {
    public:
        UserInput(App& app);
        
        struct KeyMappings {
            int moveLeft     = GLFW_KEY_A;
            int moveRight    = GLFW_KEY_D;
            int moveForward  = GLFW_KEY_W;
            int moveBackward = GLFW_KEY_S;
            int moveUp       = GLFW_KEY_E;
            int moveDown     = GLFW_KEY_Q;
            int lookLeft     = GLFW_KEY_LEFT;
            int lookRight    = GLFW_KEY_RIGHT;
            int lookUp       = GLFW_KEY_UP;
            int lookDown     = GLFW_KEY_DOWN;
        };

        void processInput(GLFWwindow* window, float dt, ArxGameObject& gameObject);
        void updateCameraVectors();
        void processMouseMovement();
        void enableImGuiInteraction();
        void disableImGuiInteraction();
        
        KeyMappings keys{};
        float moveSpeed{120.f};
        float lookSpeed{0.05f};
        double xpos;
        double ypos;
        glm::vec3 forwardDir{0.f, 0.f, -1.f};
        glm::vec3 rightDir{1.f, 0.f, 0.f};
        glm::vec3 upDir{0.f, -1.f, 0.f};
        glm::vec3 worldUp{0.f, -1.f, 0.f};
        glm::vec3 rotate{0};
        float yaw{-90.f};
        float pitch{0.f};
        
        App& app;
        
    private:
        static UserInput* instance;
        static double xoffset;
        static double yoffset;
        static double lastX;
        static double lastY;
        static void mouse_callback(GLFWwindow* window, double xpos, double ypos);
        bool isImGuiActive = false;
        static bool wasImGuiActiveLastFrame;
    };
}
