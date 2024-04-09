#include <stdio.h>
#include "arx_camera.h"

// std
#include <cassert>
#include <limits>

namespace arx {
    void ArxCamera::setOrthographicProjection(float left, float right, float top, float bottom, float near, float far) {
        projectionMatrix = glm::mat4{1.0f};
        projectionMatrix[0][0] = 2.f / (right - left);
        projectionMatrix[1][1] = 2.f / (bottom - top);
        projectionMatrix[2][2] = 1.f / (far - near);
        projectionMatrix[3][0] = -(right + left) / (right - left);
        projectionMatrix[3][1] = -(bottom + top) / (bottom - top);
        projectionMatrix[3][2] = -near / (far - near);
    }
     
    void ArxCamera::setPerspectiveProjection(float fovy, float aspect, float near, float far) {
        assert(glm::abs(aspect - std::numeric_limits<float>::epsilon()) > 0.0f);
        const float tanHalfFovy = tan(fovy / 2.f);
        projectionMatrix = glm::mat4{0.0f};
        projectionMatrix[0][0] = 1.f / (aspect * tanHalfFovy);
        projectionMatrix[1][1] = 1.f / (tanHalfFovy);
        projectionMatrix[2][2] = far / (far - near);
        projectionMatrix[2][3] = 1.f;
        projectionMatrix[3][2] = -(far * near) / (far - near);
    }

    void ArxCamera::setViewDirection(glm::vec3 position, glm::vec3 direction, glm::vec3 up) {
        const glm::vec3 w{glm::normalize(direction)};
        const glm::vec3 u{glm::normalize(glm::cross(w, up))};
        const glm::vec3 v{glm::cross(w, u)};

        viewMatrix = glm::mat4{1.f};
        viewMatrix[0][0] = u.x;
        viewMatrix[1][0] = u.y;
        viewMatrix[2][0] = u.z;
        viewMatrix[0][1] = v.x;
        viewMatrix[1][1] = v.y;
        viewMatrix[2][1] = v.z;
        viewMatrix[0][2] = w.x;
        viewMatrix[1][2] = w.y;
        viewMatrix[2][2] = w.z;
        viewMatrix[3][0] = -glm::dot(u, position);
        viewMatrix[3][1] = -glm::dot(v, position);
        viewMatrix[3][2] = -glm::dot(w, position);
        
        inverseViewMatrix = glm::mat4{1.f};
        inverseViewMatrix[0][0] = u.x;
        inverseViewMatrix[0][1] = u.y;
        inverseViewMatrix[0][2] = u.z;
        inverseViewMatrix[1][0] = v.x;
        inverseViewMatrix[1][1] = v.y;
        inverseViewMatrix[1][2] = v.z;
        inverseViewMatrix[2][0] = w.x;
        inverseViewMatrix[2][1] = w.y;
        inverseViewMatrix[2][2] = w.z;
        inverseViewMatrix[3][0] = position.x;
        inverseViewMatrix[3][1] = position.y;
        inverseViewMatrix[3][2] = position.z;
    }

    void ArxCamera::setViewTarget(glm::vec3 position, glm::vec3 target, glm::vec3 up) {
        setViewDirection(position, target - position, up);
    }

    void ArxCamera::lookAtRH(glm::vec3 eye, glm::vec3 center, glm::vec3 up) {
        glm::vec3 f(normalize(center - eye));
        glm::vec3 s(normalize(cross(f, up)));
        glm::vec3 u(cross(s, f));

        viewMatrix[0][0] = s.x;
        viewMatrix[1][0] = s.y;
        viewMatrix[2][0] = s.z;
        viewMatrix[0][1] = -u.x; // Invert Y axis
        viewMatrix[1][1] = -u.y;
        viewMatrix[2][1] = -u.z;
        viewMatrix[0][2] = -f.x;
        viewMatrix[1][2] = -f.y;
        viewMatrix[2][2] = -f.z;
        viewMatrix[3][0] = -glm::dot(s, eye);
        viewMatrix[3][1] =  glm::dot(u, eye); // Negate Y translation
        viewMatrix[3][2] =  glm::dot(f, eye);
        
        inverseViewMatrix = glm::mat4{1.f};
        inverseViewMatrix[0][0] = s.x;
        inverseViewMatrix[0][1] = s.y;
        inverseViewMatrix[0][2] = s.z;
        inverseViewMatrix[1][0] = u.x;
        inverseViewMatrix[1][1] = u.y;
        inverseViewMatrix[1][2] = u.z;
        inverseViewMatrix[2][0] = f.x;
        inverseViewMatrix[2][1] = f.y;
        inverseViewMatrix[2][2] = f.z;
        inverseViewMatrix[3][0] = eye.x;
        inverseViewMatrix[3][1] = eye.y;
        inverseViewMatrix[3][2] = eye.z;
    }

    void ArxCamera::cull_chunks_against_frustum(
        const std::vector<std::pair<glm::vec3, unsigned int>>& chunkPositions,
        std::vector<uint32_t>& out_visible_list, int CHUNK_SIZE) {

        glm::mat4 VP = projectionMatrix * viewMatrix;

        for (uint32_t i = 0; i < chunkPositions.size(); i++) {
            if (chunkPositions[i].second != -1) {
                if (test_AABB_against_frustum(VP, chunkPositions[i].first, CHUNK_SIZE)) {
                    out_visible_list.push_back(chunkPositions[i].second);
                    
                }
            }
        }
    }
 

    bool ArxCamera::test_AABB_against_frustum(glm::mat4& VP, const glm::vec3& chunkPosition, int CHUNK_SIZE)
    {
        // Pass the test if too close to the chunk. Frustum won't see any chunk corners
        if (glm::distance(getPosition(), chunkPosition + glm::vec3(CHUNK_SIZE)*0.5f) < 10.0f)
            return true;
            
        AABB chunkAABB;
        chunkAABB.min = chunkPosition;
        chunkAABB.max = chunkPosition + glm::vec3(CHUNK_SIZE);

        glm::vec4 corners[8] = {
            {chunkAABB.min.x, chunkAABB.min.y, chunkAABB.min.z, 1.0},
            {chunkAABB.max.x, chunkAABB.min.y, chunkAABB.min.z, 1.0},
            {chunkAABB.min.x, chunkAABB.max.y, chunkAABB.min.z, 1.0},
            {chunkAABB.max.x, chunkAABB.max.y, chunkAABB.min.z, 1.0},
            {chunkAABB.min.x, chunkAABB.min.y, chunkAABB.max.z, 1.0},
            {chunkAABB.max.x, chunkAABB.min.y, chunkAABB.max.z, 1.0},
            {chunkAABB.min.x, chunkAABB.max.y, chunkAABB.max.z, 1.0},
            {chunkAABB.max.x, chunkAABB.max.y, chunkAABB.max.z, 1.0}
        };

        // Perform the frustum culling check
        bool inside = false;
        for (size_t corner_idx = 0; corner_idx < sizeof(corners)/sizeof(corners[0]); corner_idx++) {
            glm::vec4 corner = VP * corners[corner_idx];
            inside = inside ||
                (within(-corner.w, corner.x, corner.w) &&
                 within(-corner.w, corner.y, corner.w) &&
                 within(0.0f, corner.z, corner.w));
        }
        return inside;
    }

    bool ArxCamera::within(float lower, float value, float upper)
    {
        return value >= lower && value <= upper;
    }
}
