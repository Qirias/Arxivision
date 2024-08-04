#include "svo.hpp"
#include <algorithm>
#include <stdexcept>

namespace arx {

    SVONode::SVONode(const glm::vec3& min, const glm::vec3& max)
        : min(min), max(max), chunkData(nullptr) {}

    void SVONode::split() {
        if (!isLeaf()) return;

        glm::vec3 center = (min + max) * 0.5f;
        children.reserve(8);

        for (int i = 0; i < 8; ++i) {
            glm::vec3 childMin = min;
            glm::vec3 childMax = center;

            if (i & 1) { childMin.x = center.x; childMax.x = max.x; }
            if (i & 2) { childMin.y = center.y; childMax.y = max.y; }
            if (i & 4) { childMin.z = center.z; childMax.z = max.z; }

            children.push_back(std::make_unique<SVONode>(childMin, childMax));
        }
    }

    SVONode* SVONode::getChild(int index) {
        if (index < 0 || index >= children.size()) return nullptr;
        return children[index].get();
    }

    void SVONode::setChunkData(const std::vector<InstanceData>& data) {
        chunkData = std::make_unique<std::vector<InstanceData>>(data);
    }

    void SVONode::addVoxel(const glm::vec3& localPosition, const InstanceData& voxelData) {
        if (!chunkData) {
            chunkData = std::make_unique<std::vector<InstanceData>>();
        }
        int index = findVoxelIndex(localPosition);
        if (index == -1) {
            chunkData->push_back(voxelData);
        } else {
            (*chunkData)[index] = voxelData;
        }
    }

    void SVONode::removeVoxel(const glm::vec3& localPosition) {
        if (chunkData) {
            int index = findVoxelIndex(localPosition);
            if (index != -1) {
                chunkData->erase(chunkData->begin() + index);
            }
        }
    }

    const InstanceData* SVONode::getVoxel(const glm::vec3& localPosition) const {
        if (chunkData) {
            int index = findVoxelIndex(localPosition);
            if (index != -1) {
                return &(*chunkData)[index];
            }
        }
        return nullptr;
    }

    int SVONode::findVoxelIndex(const glm::vec3& localPosition) const {
        if (chunkData) {
            for (size_t i = 0; i < chunkData->size(); ++i) {
                if (glm::vec3((*chunkData)[i].translation) == localPosition) {
                    return static_cast<int>(i);
                }
            }
        }
        return -1;
    }

    SVO::SVO(const glm::vec3& worldSize, int chunkSize)
        : chunkSize(chunkSize) {
        root = std::make_unique<SVONode>(glm::vec3(0), worldSize);
    }

    void SVO::insertChunk(const glm::vec3& chunkPosition, const std::vector<InstanceData>& chunkData) {
        SVONode* node = root.get();
        glm::vec3 nodeMin = root->min;
        glm::vec3 nodeMax = root->max;

        while ((nodeMax - nodeMin).x > chunkSize) {
            if (node->isLeaf()) {
                node->split();
            }

            glm::vec3 center = (nodeMin + nodeMax) * 0.5f;
            int childIndex = 0;
            if (chunkPosition.x >= center.x) { childIndex |= 1; nodeMin.x = center.x; } else { nodeMax.x = center.x; }
            if (chunkPosition.y >= center.y) { childIndex |= 2; nodeMin.y = center.y; } else { nodeMax.y = center.y; }
            if (chunkPosition.z >= center.z) { childIndex |= 4; nodeMin.z = center.z; } else { nodeMax.z = center.z; }

            node = node->getChild(childIndex);
        }

        node->setChunkData(chunkData);
    }

    void SVO::removeChunk(const glm::vec3& chunkPosition) {
        SVONode* node = root.get();
        glm::vec3 nodeMin = root->min;
        glm::vec3 nodeMax = root->max;

        while ((nodeMax - nodeMin).x > chunkSize && !node->isLeaf()) {
            glm::vec3 center = (nodeMin + nodeMax) * 0.5f;
            int childIndex = 0;
            if (chunkPosition.x >= center.x) { childIndex |= 1; nodeMin.x = center.x; } else { nodeMax.x = center.x; }
            if (chunkPosition.y >= center.y) { childIndex |= 2; nodeMin.y = center.y; } else { nodeMax.y = center.y; }
            if (chunkPosition.z >= center.z) { childIndex |= 4; nodeMin.z = center.z; } else { nodeMax.z = center.z; }

            node = node->getChild(childIndex);
            if (!node) return;  // Chunk doesn't exist
        }

        if (node) node->setChunkData(std::vector<InstanceData>());  // Set to empty chunk
    }

    const std::vector<InstanceData>* SVO::getChunk(const glm::vec3& chunkPosition) const {
        SVONode* node = root.get();
        glm::vec3 nodeMin = root->min;
        glm::vec3 nodeMax = root->max;

        while ((nodeMax - nodeMin).x > chunkSize && !node->isLeaf()) {
            glm::vec3 center = (nodeMin + nodeMax) * 0.5f;
            int childIndex = 0;
            if (chunkPosition.x >= center.x) { childIndex |= 1; nodeMin.x = center.x; } else { nodeMax.x = center.x; }
            if (chunkPosition.y >= center.y) { childIndex |= 2; nodeMin.y = center.y; } else { nodeMax.y = center.y; }
            if (chunkPosition.z >= center.z) { childIndex |= 4; nodeMin.z = center.z; } else { nodeMax.z = center.z; }

            node = node->getChild(childIndex);
            if (!node) return nullptr;  // Chunk doesn't exist
        }

        return node ? node->getChunkData() : nullptr;
    }

    void SVO::addVoxel(const glm::vec3& worldPosition, const InstanceData& voxelData) {
        auto [node, localPos] = getChunkNodeAndLocalPosition(worldPosition);
        if (node) {
            node->addVoxel(localPos, voxelData);
        }
    }

    void SVO::removeVoxel(const glm::vec3& worldPosition) {
        auto [node, localPos] = getChunkNodeAndLocalPosition(worldPosition);
        if (node) {
            node->removeVoxel(localPos);
        }
    }

    const InstanceData* SVO::getVoxel(const glm::vec3& worldPosition) const {
        auto [node, localPos] = getChunkNodeAndLocalPosition(worldPosition);
        if (node) {
            return node->getVoxel(localPos);
        }
        return nullptr;
    }

    std::pair<SVONode*, glm::vec3> SVO::getChunkNodeAndLocalPosition(const glm::vec3& worldPosition) const {
        SVONode* node = root.get();
        glm::vec3 nodeMin = root->min;
        glm::vec3 nodeMax = root->max;

        while ((nodeMax - nodeMin).x > chunkSize && !node->isLeaf()) {
            glm::vec3 center = (nodeMin + nodeMax) * 0.5f;
            int childIndex = 0;
            if (worldPosition.x >= center.x) { childIndex |= 1; nodeMin.x = center.x; } else { nodeMax.x = center.x; }
            if (worldPosition.y >= center.y) { childIndex |= 2; nodeMin.y = center.y; } else { nodeMax.y = center.y; }
            if (worldPosition.z >= center.z) { childIndex |= 4; nodeMin.z = center.z; } else { nodeMax.z = center.z; }

            node = node->getChild(childIndex);
            if (!node) return {nullptr, glm::vec3(0)};
        }

        glm::vec3 localPosition = worldPosition - nodeMin;
        return {node, localPosition};
    }

    void SVO::flattenSVO(SVONode* node, std::vector<GPUNode>& nodeBuffer, std::vector<InstanceData>& voxelBuffer, uint32_t& nextChildIndex) {
        GPUNode gpuNode;
        gpuNode.min = node->min;
        gpuNode.max = node->max;
        gpuNode.voxelStartIndex = static_cast<uint32_t>(voxelBuffer.size());
        
        if (node->chunkData) {
            voxelBuffer.insert(voxelBuffer.end(), node->chunkData->begin(), node->chunkData->end());
        }
        
        if (node->isLeaf()) {
            gpuNode.childrenStartIndex = 0;
        } else {
            gpuNode.childrenStartIndex = nextChildIndex;
            nextChildIndex += 8;  // Reserve space for 8 children
        }
        
        uint32_t currentNodeIndex = static_cast<uint32_t>(nodeBuffer.size());
        nodeBuffer.push_back(gpuNode);
        
        if (!node->isLeaf()) {
            for (int i = 0; i < 8; ++i) {
                flattenSVO(node->getChild(i), nodeBuffer, voxelBuffer, nextChildIndex);
            }
        }
    }

    std::pair<std::vector<GPUNode>, std::vector<InstanceData>> SVO::getFlattenedData() {
        std::vector<GPUNode> nodeBuffer;
        std::vector<InstanceData> voxelBuffer;
        uint32_t nextChildIndex = 0;
        
        flattenSVO(root.get(), nodeBuffer, voxelBuffer, nextChildIndex);
        
        return {nodeBuffer, voxelBuffer};
    }
}
