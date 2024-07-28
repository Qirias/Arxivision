#pragma once

#include "chunks.h"
#include "arx_pipeline.h"
#include "arx_frame_info.h"
#include "arx_camera.h"
#include "arx_model.h"
#include "arx_utils.h"

#include "ogt_vox.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>

// std
#include <cassert>
#include <cstring>
#include <unordered_map>
#include <iostream>
#include <random>

namespace arx {

    class ArxModel;

    struct VoxelData {
        uint32_t colorIndex;
        bool isAir;
    };

    class ChunkManager {
    public:
        ChunkManager(ArxDevice &device);
        ~ChunkManager();
        
        void setCamera(ArxCamera& camera) { this->camera = camera; }
        const std::vector<Chunk*>& GetChunks() const { return m_vpChunks; }
        void obj2vox(ArxGameObject::Map& voxel, const std::string& path, const float scaleFactor);
        std::vector<std::pair<glm::vec3, unsigned int>> GetChunkPositions() { return chunkPositions; }
        void MengerSponge(ArxGameObject::Map& voxel, const glm::ivec3& terrainSize);
        void setChunkAABB(const glm::vec3& position, const unsigned int chunkId);
        
        void vox2Chunks(ArxGameObject::Map& voxel, const std::string& filepath);
        
        const std::vector<std::pair<glm::vec3, unsigned int>>& getPositions() const { return chunkPositions; }
        const std::unordered_map<unsigned int, AABB>& getChunkAABBs() const { return chunkAABBs; }
        
    private:
        ArxDevice                                               &arxDevice;
        std::vector<Chunk*>                                     m_vpChunks; // All the Chunk instances
        std::unordered_map<unsigned int, AABB>                  chunkAABBs; // The world space AABB min and max of each chunk
        ArxModel::Builder                                       builder;
        ArxCamera                                               camera;
        std::vector<std::pair<glm::vec3, unsigned int>>         chunkPositions; // World space positions of chunks
        std::vector<std::vector<std::vector<VoxelData>>>        voxelWorld; // A 3D representation of each voxel including the air voxels

                
        const ogt_vox_scene* loadVoxModel(const std::string& filepath);
        void setChunkPosition(const std::pair<glm::vec3, unsigned int>& position);
        glm::mat4 ogtTransformToMat4(const ogt_vox_transform& transform);
    };
}
