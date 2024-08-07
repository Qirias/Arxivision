#include "svo.hpp"
#include <algorithm>
#include <stdexcept>
#include <iostream>

namespace arx {

    SVONode::SVONode(const glm::vec3& min, const glm::vec3& max)
    : min(min), max(max), chunkData(nullptr), childrenStartIndex(0), voxelStartIndex(0) {}

    void SVONode::split(size_t& nextNodeIndex) {
        if (!isLeaf()) return;

        glm::vec3 center = (min + max) * 0.5f;
        children.reserve(8);

        for (int i = 0; i < 8; ++i) {
            glm::vec3 childMin = min;
            glm::vec3 childMax = center;

            if (i & 1) { childMin.x = center.x; childMax.x = max.x; }
            if (i & 2) { childMin.y = center.y; childMax.y = max.y; }
            if (i & 4) { childMin.z = center.z; childMax.z = max.z; }

            auto child = std::make_unique<SVONode>(childMin, childMax);
            child->voxelStartIndex = this->voxelStartIndex + (this->chunkData ? static_cast<uint32_t>(this->chunkData->size()) : 0);
            children.push_back(std::move(child));
        }
        
        childrenStartIndex = children[0]->voxelStartIndex;
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

    SVONode* SVONode::getChild(int index) {
        if (index < 0 || index >= children.size()) return nullptr;
        return children[index].get();
    }

    void SVONode::setChunkData(const std::vector<InstanceData>& data) {
        chunkData = std::make_unique<std::vector<InstanceData>>(data);
    }

    void SVO::addVoxel(const glm::vec3& worldPosition, const InstanceData& voxelData) {
        auto [node, localPos] = getChunkNodeAndLocalPosition(worldPosition);
        if (node) {
            node->addVoxel(localPos, voxelData);
            
            voxels.push_back(voxelData);
            
            auto it = std::find_if(nodes.begin(), nodes.end(), [node](const GPUNode& gpuNode) {
                return gpuNode.min == node->min && gpuNode.max == node->max;
            });
            
            if (it != nodes.end()) {
                it->voxelStartIndex = static_cast<uint32_t>(voxels.size() - node->chunkData->size());
                
                for (auto nextIt = it + 1; nextIt != nodes.end(); ++nextIt) {
                    nextIt->voxelStartIndex++;
                }
            }
            
            updateChildrenStartIndices();
        }
    }

    void SVO::updateChildrenStartIndices() {
        for (size_t i = 0; i < nodes.size(); ++i) {
            if (i + 1 < nodes.size() && nodes[i + 1].voxelStartIndex > nodes[i].voxelStartIndex) {
                nodes[i].childrenStartIndex = nodes[i + 1].voxelStartIndex;
            } else {
                nodes[i].childrenStartIndex = 0;  // Leaf node
            }
        }
    }

    size_t SVONode::removeVoxel(const glm::vec3& localPosition) {
        if (chunkData) {
            for (size_t i = 0; i < chunkData->size(); ++i) {
                if (glm::vec3((*chunkData)[i].translation) == localPosition) {
                    chunkData->erase(chunkData->begin() + i);
                    return i;
                }
            }
        }
        return static_cast<size_t>(-1);  // Voxel not found
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
        : chunkSize(chunkSize), nextNodeIndex(0){
        root = std::make_unique<SVONode>(glm::vec3(0), worldSize);
        nodes.push_back({root->min, root->max, 0, 0});
    }

    void SVO::insertChunk(const glm::vec3& chunkPosition, const std::vector<InstanceData>& chunkData) {
        SVONode* node = root.get();
        glm::vec3 nodeMin = root->min;
        glm::vec3 nodeMax = root->max;

        while ((nodeMax - nodeMin).x > chunkSize) {
            if (node->isLeaf()) {
                node->split(nextNodeIndex);
                
                for (int i = 0; i < 8; ++i) {
                    SVONode* child = node->getChild(i);
                    nodes.push_back({child->min, child->max, static_cast<uint32_t>(voxels.size()), 0});
                }
            }

            glm::vec3 center = (nodeMin + nodeMax) * 0.5f;
            int childIndex = 0;
            if (chunkPosition.x >= center.x) { childIndex |= 1; nodeMin.x = center.x; } else { nodeMax.x = center.x; }
            if (chunkPosition.y >= center.y) { childIndex |= 2; nodeMin.y = center.y; } else { nodeMax.y = center.y; }
            if (chunkPosition.z >= center.z) { childIndex |= 4; nodeMin.z = center.z; } else { nodeMax.z = center.z; }

            node = node->getChild(childIndex);
        }
        
        
        node->setChunkData(chunkData);
        node->voxelStartIndex = static_cast<uint32_t>(voxels.size());
        voxels.insert(voxels.end(), chunkData.begin(), chunkData.end());

        // Update voxel start indices for all subsequent nodes
        updateVoxelStartIndices(root.get(), 0);
    }

    uint32_t SVO::updateVoxelStartIndices(SVONode* node, uint32_t startIndex) {
        node->voxelStartIndex = startIndex;
        if (node->chunkData) {
            startIndex += node->chunkData->size();
        }
        if (!node->isLeaf()) {
            node->childrenStartIndex = startIndex;
            for (auto& child : node->children) {
                startIndex = updateVoxelStartIndices(child.get(), startIndex);
            }
        }
        return startIndex;
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

    void SVO::removeVoxel(const glm::vec3& worldPosition) {
        auto [node, localPos] = getChunkNodeAndLocalPosition(worldPosition);
        if (node) {
            size_t removedIndex = node->removeVoxel(localPos);
            if (removedIndex != static_cast<size_t>(-1)) {
                
                auto it = std::find_if(nodes.begin(), nodes.end(), [node](const GPUNode& gpuNode) {
                    return gpuNode.min == node->min && gpuNode.max == node->max;
                });
                
                if (it != nodes.end()) {
                    size_t globalVoxelIndex = it->voxelStartIndex + removedIndex;
                    
                    voxels.erase(voxels.begin() + globalVoxelIndex);

                    for (auto updateIt = it; updateIt != nodes.end(); ++updateIt) {
                        --updateIt->voxelStartIndex;
                    }

                    updateChildrenStartIndices();
                }
            }
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
}
