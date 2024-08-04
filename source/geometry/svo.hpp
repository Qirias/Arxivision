#pragma once

#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include "arx_buffer_manager.hpp"

namespace arx {

    class SVONode {
    public:
        SVONode(const glm::vec3& min, const glm::vec3& max);
        
        bool isLeaf() const { return children.empty(); }
        void split();
        SVONode* getChild(int index);
        void setChunkData(const std::vector<InstanceData>& data);
        const std::vector<InstanceData>* getChunkData() const { return chunkData.get(); }
        
        void addVoxel(const glm::vec3& localPosition, const InstanceData& voxelData);
        void removeVoxel(const glm::vec3& localPosition);
        const InstanceData* getVoxel(const glm::vec3& localPosition) const;
        
        int findVoxelIndex(const glm::vec3& localPosition) const;

        glm::vec3 min, max;
        std::vector<std::unique_ptr<SVONode>> children;
        std::unique_ptr<std::vector<InstanceData>> chunkData;
    };

    class SVO {
    public:
        SVO(const glm::vec3& worldSize, int chunkSize);
        
        void insertChunk(const glm::vec3& chunkPosition, const std::vector<InstanceData>& chunkData);
        void removeChunk(const glm::vec3& chunkPosition);
        const std::vector<InstanceData>* getChunk(const glm::vec3& chunkPosition) const;
        
        std::pair<std::vector<GPUNode>, std::vector<InstanceData>> getFlattenedData();
        void addVoxel(const glm::vec3& worldPosition, const InstanceData& voxelData);
        void removeVoxel(const glm::vec3& worldPosition);
        const InstanceData* getVoxel(const glm::vec3& worldPosition) const;

    private:
        std::unique_ptr<SVONode>    root;
        int                         chunkSize;
        
        void flattenSVO(SVONode* node,
                        std::vector<GPUNode>& nodeBuffer,
                        std::vector<InstanceData>& voxelBuffer,
                        uint32_t& nextChildIndex);
        std::pair<SVONode*, glm::vec3> getChunkNodeAndLocalPosition(const glm::vec3& worldPosition) const;
    };
}
