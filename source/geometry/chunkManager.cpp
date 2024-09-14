#include "../source/engine_pch.hpp"

#include "../source/geometry/chunkManager.h"
#include "../source/geometry/svo.hpp"
#include "../source/geometry/blockMaterials.hpp"
#include "../source/arx_frame_info.h"
#include "../source/arx_utils.h"

namespace arx {
    
    ChunkManager::ChunkManager(ArxDevice &device) : arxDevice{device} {}

    ChunkManager::~ChunkManager() {
        Materials::cleanup();
    }

    void ChunkManager::MengerSponge(ArxGameObject::Map& voxel, const glm::ivec3& terrainSize) {
        
        int numChunksX = terrainSize.x / ADJUSTED_CHUNK;
        int numChunksY = terrainSize.y / ADJUSTED_CHUNK;
        int numChunksZ = terrainSize.z / ADJUSTED_CHUNK;

        for (int x = 0; x < numChunksX; ++x) {
            for (int y = 0; y < numChunksY; ++y) {
                for (int z = 0; z < numChunksZ; ++z) {
                    glm::vec3 chunkPosition(x * ADJUSTED_CHUNK, y * ADJUSTED_CHUNK, z * ADJUSTED_CHUNK);
                    Chunk* newChunk = new Chunk(arxDevice, chunkPosition, voxel, terrainSize);
                    m_vpChunks.push_back(newChunk);
                    if (newChunk->getID() != -1) {
                        setChunkPosition({newChunk->getPosition(), newChunk->getID()});
                        setChunkAABB(newChunk->getPosition(), newChunk->getID());
                    }
                }
            }
        }
    }

    const ogt_vox_scene* ChunkManager::loadVoxModel(const std::string& filepath) {
        FILE* file = fopen(filepath.c_str(), "rb");
        if (!file) {
            // std::cerr << "Failed to open .vox file: " << filepath << std::endl;
            ARX_LOG_ERROR("Failed to open the .vox file {}", filepath);
            return nullptr;
        }

        fseek(file, 0, SEEK_END);
        uint32_t bufferSize = static_cast<uint32_t>(ftell(file));
        fseek(file, 0, SEEK_SET);

        uint8_t* buffer = new uint8_t[bufferSize];
        fread(buffer, bufferSize, 1, file);
        fclose(file);

        // Load the scene
        const ogt_vox_scene* scene = ogt_vox_read_scene(buffer, bufferSize);
        delete[] buffer;

        if (!scene) {
            std::cerr << "Failed to read .vox scene: " << filepath << std::endl;
        }

        return scene;
    }

    glm::mat4 ChunkManager::ogtTransformToMat4(const ogt_vox_transform& transform) {
        return glm::mat4(
            transform.m00, transform.m01, transform.m02, transform.m03,
            transform.m10, transform.m11, transform.m12, transform.m13,
            transform.m20, transform.m21, transform.m22, transform.m23,
            transform.m30, transform.m31, transform.m32, transform.m33
        );
    }

    bool ChunkManager::vox2Chunks(ArxGameObject::Map& voxel, const std::string& filepath) {
        const ogt_vox_scene* scene = loadVoxModel(filepath);
        if (!scene) return false;
        
        glm::mat4 worldPosMatrix = glm::mat4(1.f);
        
        const std::array<glm::ivec3, 6> faceDirections = {
            glm::ivec3(-1,  0,  0), // Left (negative X)
            glm::ivec3( 1,  0,  0), // Right (positive X)
            glm::ivec3( 0,  1,  0), // Bottom (positive Y)
            glm::ivec3( 0, -1,  0), // Top (negative Y)
            glm::ivec3( 0,  0,  1), // Back (positive Z)
            glm::ivec3( 0,  0, -1)  // Front (negative Z)
        };
        
        // Calculate world size
        glm::vec3 worldSize(0.0f);
        for (uint32_t instanceIndex = 0; instanceIndex < scene->num_instances; ++instanceIndex) {
            const ogt_vox_instance* instance = &scene->instances[instanceIndex];
            const ogt_vox_model* model = scene->models[instance->model_index];
            glm::mat4 modelTransform = ogtTransformToMat4(instance->transform);
            glm::vec3 maxBounds(model->size_x, model->size_y, model->size_z);
            glm::vec4 transformedMax = modelTransform * glm::vec4(maxBounds, 1.0f);
            worldSize = glm::max(worldSize, glm::vec3(transformedMax));
        }
        
        // Create SVO
        svo = std::make_unique<SVO>(worldSize, CHUNK_SIZE);
        std::unordered_map<uint32_t, std::vector<PointLight>> chunkLights;
        
        for (uint32_t instanceIndex = 0; instanceIndex < scene->num_instances; ++instanceIndex) {
            
            const ogt_vox_instance* instance = &scene->instances[instanceIndex];
            const ogt_vox_model* model = scene->models[instance->model_index];

            glm::mat4 modelTransform = ogtTransformToMat4(instance->transform);
            glm::vec3 maxBounds(model->size_x, model->size_y, model->size_z);

            int numChunksX = static_cast<int>(std::ceil(maxBounds.x / CHUNK_SIZE));
            int numChunksY = static_cast<int>(std::ceil(maxBounds.y / CHUNK_SIZE));
            int numChunksZ = static_cast<int>(std::ceil(maxBounds.z / CHUNK_SIZE));

            std::vector<std::vector<std::vector<std::vector<InstanceData>>>> chunks(numChunksX,
                std::vector<std::vector<std::vector<InstanceData>>>(numChunksY,
                    std::vector<std::vector<InstanceData>>(numChunksZ, std::vector<InstanceData>())));
            
            // Initialize voxelWorld with the correct dimensions
            voxelWorld.resize(model->size_x,
                std::vector<std::vector<VoxelData>>(model->size_y,
                    std::vector<VoxelData>(model->size_z, {0, true})));
            
            // First pass: Populate the voxelWorld array
            for (uint32_t voxelX = 0; voxelX < model->size_x; ++voxelX) {
                for (uint32_t voxelY = 0; voxelY < model->size_y; ++voxelY) {
                    for (uint32_t voxelZ = 0; voxelZ < model->size_z; ++voxelZ) {
                        uint32_t colorIndex = model->voxel_data[voxelX + (voxelY * model->size_x) + (voxelZ * model->size_x * model->size_y)];
                        voxelWorld[voxelX][voxelY][voxelZ] = {colorIndex, colorIndex == 0}; // 3D representation includes air voxels
                    }
                }
            }

            for (uint32_t voxelX = 0; voxelX < model->size_x; ++voxelX) {
                for (uint32_t voxelY = 0; voxelY < model->size_y; ++voxelY) {
                    for (uint32_t voxelZ = 0; voxelZ < model->size_z; ++voxelZ) {
                        uint32_t colorIndex = model->voxel_data[voxelX + (voxelY * model->size_x) + (voxelZ * model->size_x * model->size_y)];
                        if (colorIndex != 0) {
                            glm::vec4 position(voxelX, voxelY, voxelZ, 1.0f);
                            glm::vec4 worldPosition = modelTransform * position; // Transform to world space
                            glm::vec4 color = glm::vec4(scene->palette.color[colorIndex].r / 255.0f,
                                                        scene->palette.color[colorIndex].g / 255.0f,
                                                        scene->palette.color[colorIndex].b / 255.0f,
                                                        /*scene->materials.matl[colorIndex].spec*/0.5f);
                            
                            int chunkX = voxelX / CHUNK_SIZE;
                            int chunkY = voxelY / CHUNK_SIZE;
                            int chunkZ = voxelZ / CHUNK_SIZE;
                            
                            uint32_t visibilityMask = 0x3F; // All faces visible by default

                            for (int i = 0; i < 6; ++i) {
                                glm::ivec3 neighborPos(voxelX + faceDirections[i].x,
                                                       voxelY + faceDirections[i].y,
                                                       voxelZ + faceDirections[i].z);
                                
                                // Check if the neighboring voxel is within bounds and not air
                                if (neighborPos.x >= 0 && neighborPos.x < model->size_x &&
                                    neighborPos.y >= 0 && neighborPos.y < model->size_y &&
                                    neighborPos.z >= 0 && neighborPos.z < model->size_z &&
                                    !voxelWorld[neighborPos.x][neighborPos.y][neighborPos.z].isAir) {
                                    // If there's a solid voxel in this direction, hide this face
                                    visibilityMask &= ~(1u << i);
                                }
                            }
                            
                            ogt_matl_type mat = scene->materials.matl[colorIndex].type;
                            if (mat == 3) { // Emit
                                // Values of 0-1023 for each directions, otherwise I need more than 32bits
                                uint32_t chunkID = (chunkX & 0x3FF) | ((chunkY & 0x3FF) << 10) | ((chunkZ & 0x3FF) << 20);
                                
                                PointLight light;
                                light.position = glm::vec3(worldPosition);
                                light.color = color;
                                light.visibilityMask = visibilityMask;
                                visibilityMask |= (1u << 6);
                                chunkLights[chunkID].push_back(light);
                            }
        
                            chunks[chunkX][chunkY][chunkZ].push_back({worldPosition, color, visibilityMask});
                        }
                    }
                }
            }

            for (int x = 0; x < numChunksX; ++x) {
                for (int y = 0; y < numChunksY; ++y) {
                    for (int z = 0; z < numChunksZ; ++z) {
                        if (!chunks[x][y][z].empty()) {
                            glm::vec3 chunkPosition = glm::vec3(worldPosMatrix * modelTransform * glm::vec4(x * CHUNK_SIZE, y * CHUNK_SIZE, z * CHUNK_SIZE, 1.0f));
                            
                            svo->insertChunk(chunkPosition, chunks[x][y][z]);
                            
                            Chunk* newChunk = new Chunk(arxDevice, chunkPosition, voxel, chunks[x][y][z]);
                            
                            m_vpChunks.push_back(newChunk);
                            if (newChunk->getID() != -1) {
                                setChunkPosition({newChunk->getPosition(), newChunk->getID()});
                                setChunkAABB(newChunk->getPosition(), newChunk->getID());
                            }
                        }
                    }
                }
            }
        }
        
        // Set a dummy light for scenes with no light
        // My system doesn't support nulldescriptors
        
        PointLight light;
        light.position = glm::vec3(0,0,0);
        light.color = glm::vec4(1);
        light.visibilityMask = 0;
        
        if (chunkLights.size() == 0) chunkLights[0].push_back(light);
        if (chunkLights.size() > 0)
            Materials::initialize(arxDevice, chunkLights);
        BufferManager::createSVOBuffers(arxDevice, svo->getNodes(), svo->getVoxels());
        
        ogt_vox_destroy_scene(scene);
    
        return true;
    }

    void ChunkManager::setChunkPosition(const std::pair<glm::vec3, unsigned int>& position) {
            chunkPositions.push_back({position.first, position.second});
    }

    void ChunkManager::setChunkAABB(const glm::vec3& position, const unsigned int chunkId) {
        AABB aabb;
        aabb.min = position;
        aabb.max = position + glm::vec3(CHUNK_SIZE);
   
        chunkAABBs[chunkId] = aabb;
    }

    void ChunkManager::addVoxel(const glm::vec3& worldPosition, const InstanceData& voxelData) {
        svo->addVoxel(worldPosition, voxelData);
        // Update corresponding Chunk object if necessary
        // Update rendering data
    }

    void ChunkManager::removeVoxel(const glm::vec3& worldPosition) {
        svo->removeVoxel(worldPosition);
        // Update corresponding Chunk object if necessary
        // Update rendering data
    }

    const InstanceData* ChunkManager::getVoxel(const glm::vec3& worldPosition) const {
        return svo->getVoxel(worldPosition);
    }
}
