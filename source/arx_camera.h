#pragma once

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "glm/gtc/matrix_transform.hpp"

namespace arx {
    class ArxCamera {
    public:
    
        void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
        
        void setPerspectiveProjection(float fovy, float aspect, float near, float far);
        
        void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
        void setViewTarget(glm::vec3 target, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
        void setViewMatrix(glm::vec3 position, glm::vec3 front, glm::vec3 up);
        glm::mat4 lookAtRH(glm::vec3 eye, glm::vec3 center, glm::vec3 up);
        
        const glm::mat4& getProjection() const { return projectionMatrix; }
        const glm::mat4& getView () const { return viewMatrix; }
    private:
        glm::mat4 projectionMatrix{1.f};
        glm::mat4 viewMatrix{1.f};
    };
}
