#pragma once

#include "../../source/arx_pipeline.h"
#include "../../source/arx_frame_info.h"
#include "../../source/arx_game_object.h"
#include "../../source/geometry/block.h"
#include "../../source/arx_model.h"
#include "../../source/threadpool.h"

namespace arx {
    
    class ArxModel;
    
    class Chunk {
    public:
        Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, glm::ivec3 terrainSize);
        Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, const std::vector<InstanceData>& instanceDataVec);
        ~Chunk();

        void Update();
        void Render();
        glm::vec3 getPosition() const { return position; }
        unsigned int getID() const { return id; }
        uint32_t getInstanceCount() const { return instances; }
        
        void applyCARule(glm::ivec3 terrainSize);
        glm::vec3 determineColorBasedOnPosition(glm::vec3 voxelGlobalPos, glm::ivec3 terrainSize);
        int calculateRecursionDepth(glm::ivec3 terrainSize);
        bool isVoxelinSponge(int x, int y, int z, int depth);
        
    private:
        std::unique_ptr<std::unique_ptr<std::unique_ptr<Block[]>[]>[]>      blocks;
        std::unique_ptr<std::unique_ptr<std::unique_ptr<glm::vec3[]>[]>[]>  colors;
        
        glm::vec3                                                           position;
        std::map<unsigned int, std::vector<InstanceData>>                   instanceData;
        uint32_t                                                            instances = 0;
        unsigned int                                                        id = -1;

        void initializeBlocks();
    };
}
