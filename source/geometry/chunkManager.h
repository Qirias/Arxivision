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
#include <random>


namespace arx {

    class ArxModel;

    class ChunkManager {
    public:
        ChunkManager(ArxDevice &device);
        ~ChunkManager();
        
        void setCamera(ArxCamera& camera) { this->camera = camera; }
        const std::vector<Chunk*>& GetChunks() const { return m_vpChunks; }
        void obj2vox(ArxGameObject::Map& voxel, const std::string& path, const float scaleFactor);
        std::vector<std::pair<glm::vec3, unsigned int>> GetChunkPositions() { return chunkPositions; }
        void initializeTerrain(ArxGameObject::Map& voxel, const glm::ivec3& terrainSize);
        void setChunkAABB(const glm::vec3& position, const unsigned int chunkId);
        
        const std::vector<std::pair<glm::vec3, unsigned int>>& getPositions() const { return chunkPositions; }
        const std::unordered_map<unsigned int, AABB>& getChunkAABBs() const { return chunkAABBs; }
        
    private:
        ArxDevice                                               &arxDevice;
        std::vector<Chunk*>                                     m_vpChunks;
        std::unordered_map<unsigned int, AABB>                  chunkAABBs;
        ArxModel::Builder                                       builder;
        ArxCamera                                               camera;
        std::vector<std::pair<glm::vec3, unsigned int>>         chunkPositions;
                
        void setChunkPosition(const std::pair<glm::vec3, unsigned int>& position);
        
    };
}
