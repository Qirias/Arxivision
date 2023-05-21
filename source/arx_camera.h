#pragma once

// libs
#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include "glm/gtc/matrix_transform.hpp"
#include <iostream>
#include <vector>

namespace arx {
    class ArxCamera {
    public:
        
        struct AABB {
            glm::vec3 min{};
            glm::vec3 max{};
        };
        
        void cull_AABBs_against_frustum(const ArxCamera& camera,
                                        const std::vector<glm::mat4>& transforms,
                                        const std::vector<AABB>& aabb_list,
                                        std::vector<uint32_t>& out_visible_list);
        
        bool test_AABB_against_frustum(glm::mat4& MVP, const AABB& aabb);
        
        bool within(float lower, float value, float upper);
            
        void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
        
        void setPerspectiveProjection(float fovy, float aspect, float near, float far);
        
        void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
        void setViewTarget(glm::vec3 target, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
        void setViewMatrix(glm::vec3 position, glm::vec3 front, glm::vec3 up);
        glm::mat4 lookAtRH(glm::vec3 eye, glm::vec3 center, glm::vec3 up);
        
        const glm::mat4& getProjection() const { return projectionMatrix; }
        const glm::mat4& getView () const { return viewMatrix; }
        const glm::mat4& getInverseView() const { return inverseViewMatrix; }
        const glm::vec3 getPosition() const { return glm::vec3(inverseViewMatrix[3]); }
    private:
        glm::mat4 projectionMatrix{1.f};
        glm::mat4 viewMatrix{1.f};
        glm::mat4 inverseViewMatrix{1.f};
    };
}
