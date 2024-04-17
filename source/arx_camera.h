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
        
        void cull_chunks_against_frustum(const std::vector<std::pair<glm::vec3, unsigned int>>& chunkPositions, std::vector<uint32_t>& out_visible_list, int CHUNK_SIZE);
        
        bool test_AABB_against_frustum(glm::mat4& MVP, const glm::vec3& chunkPosition, int CHUNK_SIZE);
        
        bool within(float lower, float value, float upper);
            
        void setOrthographicProjection(float left, float right, float top, float bottom, float near, float far);
        
        void setPerspectiveProjection(float fovy, float aspect, float near, float far);
        
        void setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
        void setViewTarget(glm::vec3 target, glm::vec3 direction, glm::vec3 up = glm::vec3{0.f, -1.f, 0.f});
        void lookAtRH(glm::vec3 eye, glm::vec3 center, glm::vec3 up);
        
        const glm::mat4& getProjection() const { return projectionMatrix; }
        const glm::mat4& getView () const { return viewMatrix; }
        const glm::mat4& getInverseView() const { return inverseViewMatrix; }
        const glm::vec3 getPosition() const { return glm::vec3(inverseViewMatrix[3]); }
        const glm::mat4 getVP() const { return glm::mat4(projectionMatrix * viewMatrix); }
    private:
        glm::mat4 projectionMatrix{1.f};
        glm::mat4 viewMatrix{1.f};
        glm::mat4 inverseViewMatrix{1.f};
    };
}
