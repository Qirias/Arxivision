#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "arx_buffer_manager.hpp"

namespace arx {

    class SVONode {
    public:
        SVONode(const glm::vec3& min, const glm::vec3& max, size_t index);
        
        bool isLeaf() const { return children.empty(); }
        void split(size_t& nextNodeIndex);
        SVONode* getChild(int index);
        void setChunkData(const std::vector<InstanceData>& data);
        const std::vector<InstanceData>* getChunkData() const { return chunkData.get(); }
        
        void addVoxel(const glm::vec3& localPosition, const InstanceData& voxelData);
        size_t removeVoxel(const glm::vec3& localPosition);
        const InstanceData* getVoxel(const glm::vec3& localPosition) const;
        
        int findVoxelIndex(const glm::vec3& localPosition) const;

        glm::vec3                                   min;
        glm::vec3                                   max;
        size_t                                      index;
        std::vector<std::unique_ptr<SVONode>>       children;
        std::unique_ptr<std::vector<InstanceData>>  chunkData;
    };

    class SVO {
    public:
        SVO(const glm::vec3& worldSize, int chunkSize);
        
        void insertChunk(const glm::vec3& chunkPosition, const std::vector<InstanceData>& chunkData);
        void removeChunk(const glm::vec3& chunkPosition);
        const std::vector<InstanceData>* getChunk(const glm::vec3& chunkPosition) const;
        
        void addVoxel(const glm::vec3& worldPosition, const InstanceData& voxelData);
        void removeVoxel(const glm::vec3& worldPosition);
        const InstanceData* getVoxel(const glm::vec3& worldPosition) const;

        const std::vector<GPUNode>& getNodes() const { return nodes; }
        const std::vector<InstanceData>& getVoxels() const { return voxels; }

    private:
        std::unique_ptr<SVONode>    root;
        int                         chunkSize;
        std::vector<GPUNode>        nodes;
        std::vector<InstanceData>   voxels;
        size_t                      nextNodeIndex;
        
        std::pair<SVONode*, glm::vec3> getChunkNodeAndLocalPosition(const glm::vec3& worldPosition) const;
    };
}
