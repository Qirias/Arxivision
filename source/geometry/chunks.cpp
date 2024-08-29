#include "../source/engine_pch.hpp"

#include "../source/geometry/chunks.h"

namespace arx {
    Chunk::Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, glm::ivec3 terrainSize) : position{pos} {
        initializeBlocks();

        std::vector<InstanceData> tmpInstance;
        applyCARule(terrainSize);
        tmpInstance.resize(instances);

        uint32_t instances = 0;
        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                    if (!blocks[x][y][z].isActive()) continue;
                    glm::vec3 translation = glm::vec3(x*VOXEL_SIZE, y*VOXEL_SIZE, z*VOXEL_SIZE) + position;
                    tmpInstance[instances].color = glm::vec4(colors[x][y][z], 1.0f);
                    tmpInstance[instances].translation = glm::vec4(translation, 1.0f);
                    instances++;
                }
            }
        }

//        std::cout << "Instances drawn: " << instances << std::endl;
        if (instances > 0)
        {
            std::shared_ptr<ArxModel> cubeModel = ArxModel::createModelFromFile(device, "data/models/cube.obj", instances, tmpInstance);
            auto cube = ArxGameObject::createGameObject();
            cube.model = cubeModel;
            id = cube.getId();
            voxel.emplace(id, std::move(cube));
            instanceData[id] = tmpInstance;
        }
    }

    Chunk::Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& voxel, const std::vector<InstanceData>& instanceDataVec) : position{pos} {
        instances = static_cast<uint32_t>(instanceDataVec.size());
        
        if (instances > 0)
        {
            std::shared_ptr<ArxModel> cubeModel = ArxModel::createModelFromFile(device, "data/models/cube.obj", instances, instanceDataVec);
            auto cube = ArxGameObject::createGameObject();
            id = cube.getId();
            cube.model = cubeModel;
            voxel.emplace(id, std::move(cube));
            instanceData[id] = instanceDataVec;
        }
    }

    void Chunk::initializeBlocks() {
        blocks = std::make_unique<std::unique_ptr<std::unique_ptr<Block[]>[]>[]>(ADJUSTED_CHUNK);
        colors = std::make_unique<std::unique_ptr<std::unique_ptr<glm::vec3[]>[]>[]>(ADJUSTED_CHUNK);
        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            blocks[x] = std::make_unique<std::unique_ptr<Block[]>[]>(ADJUSTED_CHUNK);
            colors[x] = std::make_unique<std::unique_ptr<glm::vec3[]>[]>(ADJUSTED_CHUNK);
            for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                blocks[x][y] = std::make_unique<Block[]>(ADJUSTED_CHUNK);
                colors[x][y] = std::make_unique<glm::vec3[]>(ADJUSTED_CHUNK);
                for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                    blocks[x][y][z].setActive(false);
                    colors[x][y][z] = glm::vec3(0);
                }
            }
        }
    }


    void Chunk::applyCARule(glm::ivec3 terrainSize) {
        int recursionDepth = calculateRecursionDepth(terrainSize);

        for (int x = 0; x < ADJUSTED_CHUNK; x++) {
            for (int y = 0; y < ADJUSTED_CHUNK; y++) {
                for (int z = 0; z < ADJUSTED_CHUNK; z++) {
                    glm::vec3 voxelGlobalPos = position + glm::vec3(x, y, z);

                    if (isVoxelinSponge(voxelGlobalPos.x, voxelGlobalPos.y, voxelGlobalPos.z, recursionDepth)) {
                        blocks[x][y][z].setActive(true);
                        instances++;

                        colors[x][y][z] = determineColorBasedOnPosition(voxelGlobalPos, terrainSize);
                    }
                }
            }
        }
    }


    glm::vec3 Chunk::determineColorBasedOnPosition(glm::vec3 voxelGlobalPos, glm::ivec3 terrainSize) {
        glm::vec3 color1(0.8f, 0.1f, 0.3f);
        glm::vec3 color2(0.1f, 0.8f, 0.3f);
        glm::vec3 color3(0.1f, 0.3f, 0.8f);

        glm::vec3 normalizedPos = voxelGlobalPos / glm::vec3(terrainSize);

        glm::vec3 modulatedColor = glm::mix(glm::mix(color1, color2, normalizedPos.x),
                                            color3,
                                            normalizedPos.y);

        modulatedColor = glm::clamp(modulatedColor, 0.0f, 1.0f);

        return modulatedColor;
    }

    int Chunk::calculateRecursionDepth(glm::ivec3 terrainSize) {
        int minSize = (terrainSize.x*ADJUSTED_CHUNK);
        int depth = -1;

        while (minSize >= pow(3, depth)) {
            depth++;
        }

        return depth;
    }

    bool Chunk::isVoxelinSponge(int x, int y, int z, int depth) {
        if (depth == 0 ||
            (x % 3 == 1 && y % 3 == 1) ||
            (x % 3 == 1 && z % 3 == 1) ||
            (y % 3 == 1 && z % 3 == 1) ||
            (x % 3 == 1 && y % 3 == 1 && z % 3 == 1)) {
            return false;
        }

        // If we're not at the base level, and this isn't a center piece, recurse towards the base level
        if (x > 0 || y > 0 || z > 0) {
            return isVoxelinSponge(x / 3, y / 3, z / 3, depth - 1);
        }

        return true;
    }

    Chunk::~Chunk() {}
}
