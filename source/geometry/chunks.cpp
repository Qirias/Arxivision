#include "chunks.h"

#include <iostream>

namespace arx {

    Chunk::Chunk(ArxDevice &device, const glm::vec3& pos, ArxGameObject::Map& gameObjects) : position{pos} {
        blocks = new Block **[CHUNK_SIZE];
        for (int i = 0; i < CHUNK_SIZE; i++) {
            blocks[i] = new Block *[CHUNK_SIZE];
            for (int j = 0; j < CHUNK_SIZE; j++) {
                blocks[i][j] = new Block[CHUNK_SIZE];
                for (int k = 0; k < CHUNK_SIZE; k++) {
        //                           if (j >= CHUNK_SIZE / 2)
        //                               blocks[i][j][k].setActive(false);
                }
            }
        }
        
        std::vector<InstanceData> instanceData;
        instanceData.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
        
        uint32_t instances = 0;
        
        for (int x = 0; x < CHUNK_SIZE; x++) {
            for (int y = 0; y < CHUNK_SIZE; y++) {
                for (int z = 0; z < CHUNK_SIZE; z++) {
                    int index = x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
                    if (!blocks[x][y][z].isActive()) continue;
                    instanceData[index].color = glm::vec3(x / static_cast<float>(CHUNK_SIZE),
                                                          y / static_cast<float>(CHUNK_SIZE),
                                                          z / static_cast<float>(CHUNK_SIZE));
                    instanceData[index].translation = glm::vec3(static_cast<float>(x) + position.x,
                                                                static_cast<float>(y) + position.y,
                                                                static_cast<float>(z) + position.z);
                    
                    instances++;
                }
            }
        }
        
//        std::cout << "Instances drawn: " << instances << std::endl;
        
        std::shared_ptr<ArxModel> cubeModel = ArxModel::createModelFromFile(device, "models/cube.obj", instances, instanceData);
        auto cube = ArxGameObject::createGameObject();
        cube.model = cubeModel;
        gameObjects.emplace(cube.getId(), std::move(cube));
    }

    Chunk::~Chunk() {
//        for (int i = 0; i < CHUNK_SIZE; i++) {
//            for (int j = 0; j < CHUNK_SIZE; j++) {
//                delete [] blocks[i][j];
//            }
//            delete[] blocks[i];
//        }
//        delete[] blocks;
    }

    void Chunk::CreateMesh() {
//        std::vector<InstanceData> instanceData;
//        instanceData.resize(CHUNK_SIZE * CHUNK_SIZE * CHUNK_SIZE);
//
//        uint32_t instances = 0;
//
//        for (int x = 0; x < CHUNK_SIZE; x++) {
//            for (int y = 0; y < CHUNK_SIZE; y++) {
//                for (int z = 0; z < CHUNK_SIZE; z++) {
//                    int index = x + y * CHUNK_SIZE + z * CHUNK_SIZE * CHUNK_SIZE;
//                    if (!blocks[x][y][z].isActive()) continue;
//                    instanceData[index].color = glm::vec3(x / static_cast<float>(CHUNK_SIZE),
//                                                          y / static_cast<float>(CHUNK_SIZE),
//                                                          z / static_cast<float>(CHUNK_SIZE));
//                    instanceData[index].translation = glm::vec3(static_cast<float>(x),
//                                                                static_cast<float>(y),
//                                                                static_cast<float>(z));
//
//                    instances++;
//                }
//            }
//        }
//
//        std::cout << "Instances drawn: " << instances << std::endl;
//
//        std::shared_ptr<ArxModel> cubeModel = ArxModel::createModelFromFile(arxDevice, "models/cube.obj", instances, instanceData);
//        auto cube = ArxGameObject::createGameObject();
//        cube.model = cubeModel;
//        gameObjects.emplace(cube.getId(), std::move(cube));
    }
}
