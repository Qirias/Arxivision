#pragma once

#include "../../source/geometry/chunks.h"
#include "../../source/arx_pipeline.h"
#include "../../source/arx_camera.h"
#include "../../source/arx_model.h"

#include "../../libs/ogt_vox.h"

namespace arx {

    class ArxModel;
    class SVO;
    class Materials;

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
        std::vector<std::pair<glm::vec3, unsigned int>> GetChunkPositions() { return chunkPositions; }
        void MengerSponge(ArxGameObject::Map& voxel, const glm::ivec3& terrainSize);
        void setChunkAABB(const glm::vec3& position, const unsigned int chunkId);
        
        void vox2Chunks(ArxGameObject::Map& voxel, const std::string& filepath);
        
        const std::vector<std::pair<glm::vec3, unsigned int>>& getPositions() const { return chunkPositions; }
        const std::unordered_map<unsigned int, AABB>& getChunkAABBs() const { return chunkAABBs; }
        
        // SVO related
        void addVoxel(const glm::vec3& worldPosition, const InstanceData& voxelData);

        void removeVoxel(const glm::vec3& worldPosition);

        const InstanceData* getVoxel(const glm::vec3& worldPosition) const;
        
    private:
        ArxDevice                                               &arxDevice;
        std::vector<Chunk*>                                     m_vpChunks; // All the Chunk instances
        std::unordered_map<unsigned int, AABB>                  chunkAABBs; // The world space AABB min and max of each chunk
        ArxCamera                                               camera;
        std::vector<std::pair<glm::vec3, unsigned int>>         chunkPositions; // World space positions of chunks
        std::vector<std::vector<std::vector<VoxelData>>>        voxelWorld; // A 3D representation of each voxel including the air voxels

                
        const ogt_vox_scene* loadVoxModel(const std::string& filepath);
        void setChunkPosition(const std::pair<glm::vec3, unsigned int>& position);
        glm::mat4 ogtTransformToMat4(const ogt_vox_transform& transform);
        
        std::unique_ptr<SVO>                                    svo;
    };
}
