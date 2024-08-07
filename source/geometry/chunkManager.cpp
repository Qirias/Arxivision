#include "chunkManager.h"

namespace arx {
    
    ChunkManager::ChunkManager(ArxDevice &device) : arxDevice{device} {}

    ChunkManager::~ChunkManager() {}

    void ChunkManager::obj2vox(ArxGameObject::Map& voxel, const std::string& path, const float scaleFactor) {
        builder.loadModel(path);

        glm::dmat4 viewMatrix = camera.getView();
        glm::dmat4 projectionMatrix = camera.getProjection();

        glm::vec3 minBounds = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 maxBounds = glm::vec3(std::numeric_limits<float>::lowest());

        // Transform vertices and normals into world space
        for (int i = 0; i < builder.vertices.size(); i++) {

            glm::vec4 localVertex = glm::vec4(builder.vertices[i].position * scaleFactor, 1.0f);
            localVertex.y *= -1;
            glm::vec3 localNormal = builder.vertices[i].normal;
            localNormal.y *= -1;

            // Transform vertex to world space
            glm::vec4 worldVertex = viewMatrix * localVertex;
            worldVertex = projectionMatrix * worldVertex;

            builder.vertices[i].position = glm::vec3(worldVertex);

            // Transform normal to world space
            glm::mat3 normalMatrix = glm::transpose(glm::inverse(glm::mat3(viewMatrix))); // Normal matrix for transforming normals
            builder.vertices[i].normal = glm::normalize(normalMatrix * localNormal);

            minBounds = glm::min(minBounds, builder.vertices[i].position);
            maxBounds = glm::max(maxBounds, builder.vertices[i].position);
//            std::cout << builder.vertices[i].position.x << " " << builder.vertices[i].position.y << " " << builder.vertices[i].position.z << "\n";
        }

        glm::vec3 modelSize = glm::ceil(maxBounds - minBounds);

        // Calculate chunk dimensions based on scaled model size
        int numChunksX = static_cast<int>(std::ceil(modelSize.x / CHUNK_SIZE));
        int numChunksY = static_cast<int>(std::ceil(modelSize.y / CHUNK_SIZE));
        int numChunksZ = static_cast<int>(std::ceil(modelSize.z / CHUNK_SIZE));
        std::cout << "ChunksX: " << numChunksX << " ChunksY: " << numChunksY << " ChunksZ: " << numChunksZ << "\n";
        std::cout << "Number of Chunks to Load: " << numChunksX*numChunksY*numChunksZ << "\n";
        
        int chunkNum = 0;
        // Create chunks
        for (int x = 0; x < numChunksX; ++x) {
            for (int y = 0; y < numChunksY; ++y) {
                for (int z = 0; z < numChunksZ; ++z) {
                    glm::vec3 chunkPosition = minBounds + glm::vec3(x * CHUNK_SIZE, y * CHUNK_SIZE, z * CHUNK_SIZE);
//                    std::cout << "Chunk Position: " << "(" << x << ", " << y << ", " << z << "): " << chunkPosition.x << ", " << chunkPosition.y << ", " << chunkPosition.z << std::endl;

                    // Extract vertices within the current chunk
                    std::vector<arx::ArxModel::Vertex> chunkVertices;
                    for (const auto& vertex : builder.vertices) {
                        if (vertex.position.x >= chunkPosition.x && vertex.position.x < chunkPosition.x + CHUNK_SIZE &&
                            vertex.position.y >= chunkPosition.y && vertex.position.y < chunkPosition.y + CHUNK_SIZE &&
                            vertex.position.z >= chunkPosition.z && vertex.position.z < chunkPosition.z + CHUNK_SIZE) {
                            chunkVertices.push_back(vertex);
                        }
                    }
                    std::cout << "Chunk number: " << ++chunkNum << "\n";
                    Chunk* newChunk = new Chunk(arxDevice, chunkPosition, voxel, chunkVertices);
                    m_vpChunks.push_back(newChunk);
                    setChunkPosition({chunkPosition, newChunk->getID()});
                    setChunkAABB(newChunk->getPosition(), newChunk->getID());
                }
            }
        }
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
            std::cerr << "Failed to open .vox file: " << filepath << std::endl;
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

    void ChunkManager::vox2Chunks(ArxGameObject::Map& voxel, const std::string& filepath) {
        const ogt_vox_scene* scene = loadVoxModel(filepath);
        if (!scene) return;
            
        // Rotate the model around X because the way I create the chunks is different from MagicaVoxel
        glm::mat4 worldRotation = glm::rotate(glm::mat4(1.f), glm::radians(90.f), glm::vec3(1, 0, 0)); // World space rotation
        
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
                            glm::vec4 rotatedPosition = worldRotation * worldPosition; // Apply world space rotation
                            glm::vec4 color = glm::vec4(scene->palette.color[colorIndex].r / 255.0f,
                                                        scene->palette.color[colorIndex].g / 255.0f,
                                                        scene->palette.color[colorIndex].b / 255.0f,
                                                        scene->palette.color[colorIndex].a / 255.0f);
                            
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
                            
                            // Set the leftmost bit to indicate that the rotationX90 should be applied in the gbuffer vertex shader
                            visibilityMask |= 0x80000000; // Set the 31st bit to 1

                            int chunkX = voxelX / CHUNK_SIZE;
                            int chunkY = voxelY / CHUNK_SIZE;
                            int chunkZ = voxelZ / CHUNK_SIZE;
//                            scene->materials.matl[1].type
        
                            chunks[chunkX][chunkY][chunkZ].push_back({rotatedPosition, color, visibilityMask});
                        }
                    }
                }
            }

            for (int x = 0; x < numChunksX; ++x) {
                for (int y = 0; y < numChunksY; ++y) {
                    for (int z = 0; z < numChunksZ; ++z) {
                        if (!chunks[x][y][z].empty()) {
                            glm::vec3 chunkPosition = glm::vec3(worldRotation * modelTransform * glm::vec4(x * CHUNK_SIZE, y * CHUNK_SIZE, z * CHUNK_SIZE, 1.0f));
                            
                            svo->insertChunk(chunkPosition, chunks[x][y][z]);
                            
                            Chunk* newChunk = new Chunk(arxDevice, chunkPosition, voxel, chunks[x][y][z]);
                            m_vpChunks.push_back(newChunk);
                            if (newChunk->getID() != -1) {
                                setChunkPosition({newChunk->getPosition(), newChunk->getID()});
                                
//                                        Original AABB box
//                                         +-----------+
//                                        /|          /|
//                                       / |         / |
//                                      /  |        /  |
//                                     +-----------O   |
//                                     |   X-------|---+
//                                     |  /        |  /    z
//                                     | /         | /    ^                     
//                                     |/          |/     |
//                                     +-----------+      ----> x
//                                X = MAX and O = MIN
                                
                                
// ====================================================================================================
// Since we rotate the whole .vox around the X axis, we need to adjust the AABB min and max accordingly
// ====================================================================================================
                                
//                                        Altered AABB box
//                                         X-----------+
//                                        /|          /|
//                                       / |         / |
//                                      /  |        /  |
//                                     +-----------+   |
//                                     |   +-------|---+
//                                     |  /        |  /    z
//                                     | /         | /    ^
//                                     |/          |/     |
//                                     +-----------O      ----> x
//                                X = MAX and O = MIN
                                
                                AABB aabb;
                                aabb.min = newChunk->getPosition() - glm::vec3(0, CHUNK_SIZE, 0);
                                aabb.max = newChunk->getPosition() + glm::vec3(CHUNK_SIZE, 0, CHUNK_SIZE);
                           
                                chunkAABBs[newChunk->getID()] = aabb;
                            }
                        }
                    }
                }
            }
        }
        
        BufferManager::createSVOBuffers(arxDevice, svo->getNodes(), svo->getVoxels());
//        BufferManager::printVoxelData(arxDevice);
        
        ogt_vox_destroy_scene(scene);
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
