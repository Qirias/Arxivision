#pragma once

#include "chunks.h"
#include "arx_pipeline.h"
#include "arx_frame_info.h"
#include "arx_camera.h"
#include "arx_model.h"
#include "arx_utils.h"

#include <glm/gtc/matrix_transform.hpp>

// std
#include <cassert>
#include <cstring>
#include <unordered_map>
#include <iostream>


namespace arx {

    class ArxModel;

    class ChunkManager {
    public:
        ChunkManager(ArxDevice &device);
        ~ChunkManager();
        
        void setCamera(ArxCamera& camera) { this->camera = camera; }
        void Update(ArxGameObject::Map& voxel, const glm::vec3& playerPosition);
        Chunk* CreateChunk(ArxGameObject::Map& voxel, const glm::vec3& position, std::vector<ArxModel::Vertex>& vertices);
        bool IsPointInArea(const glm::vec3& point, const glm::vec3& areaCenter, float areaSize) const;
        glm::vec3 CalculateAdjacentChunkPosition(const glm::vec3& position) const;
        const std::vector<Chunk*>& GetChunks() const { return m_vpChunks; }
        bool IsChunkAtPosition(const glm::vec3& position) const;
        void processBuilder(ArxGameObject::Map& voxel);
        
    private:
        ArxDevice                   &arxDevice;
        std::vector<Chunk*>         m_vpChunks;
        ArxModel::Builder           builder;
        ArxCamera                   camera;
        
        bool HasChunkAtPosition(const glm::vec3& position) const;
    };
}
