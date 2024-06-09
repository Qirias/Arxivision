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

    void ChunkManager::initializeTerrain(ArxGameObject::Map& voxel, const glm::ivec3& terrainSize) {
        
        int numChunksX = (terrainSize.x) / ADJUSTED_CHUNK;
        int numChunksY = (terrainSize.y) / ADJUSTED_CHUNK;
        int numChunksZ = (terrainSize.z) / ADJUSTED_CHUNK;

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

    void ChunkManager::vox2Chunks(ArxGameObject::Map& voxel, const std::string& filepath) {
        const ogt_vox_scene* scene = loadVoxModel(filepath);
        if (!scene) return;

        for (uint32_t modelIndex = 0; modelIndex < scene->num_models; ++modelIndex) {
            const ogt_vox_model* model = scene->models[modelIndex];
            glm::vec3 maxBounds(model->size_x, model->size_y, model->size_z);

            int numChunksX = static_cast<int>(std::ceil(maxBounds.x / CHUNK_SIZE));
            int numChunksY = static_cast<int>(std::ceil(maxBounds.y / CHUNK_SIZE));
            int numChunksZ = static_cast<int>(std::ceil(maxBounds.z / CHUNK_SIZE));

            for (int x = 0; x < numChunksX; ++x) {
                for (int y = 0; y < numChunksY; ++y) {
                    for (int z = 0; z < numChunksZ; ++z) {
                        glm::vec3 chunkPosition(x * CHUNK_SIZE, y * CHUNK_SIZE, z * CHUNK_SIZE);
                        std::vector<InstanceData> chunkInstanceData;

                        for (uint32_t voxelX = 0; voxelX < model->size_x; ++voxelX) {
                            for (uint32_t voxelY = 0; voxelY < model->size_y; ++voxelY) {
                                for (uint32_t voxelZ = 0; voxelZ < model->size_z; ++voxelZ) {
                                    uint32_t colorIndex = model->voxel_data[voxelX + voxelY * model->size_x + voxelZ * model->size_x * model->size_y];
                                    if (colorIndex != 0) {
                                        glm::vec4 position(voxelX, voxelY, voxelZ, 1.0f);
//                                        position = rotationMatrix * position;
                                        glm::vec4 color = glm::vec4(scene->palette.color[colorIndex].r / 255.0f,
                                                                    scene->palette.color[colorIndex].g / 255.0f,
                                                                    scene->palette.color[colorIndex].b / 255.0f,
                                                                    scene->palette.color[colorIndex].a / 255.0f);

                                        // Add voxel to chunk if it fits within the chunk bounds
                                        if (position.x >= chunkPosition.x && position.x < chunkPosition.x + CHUNK_SIZE &&
                                            position.y >= chunkPosition.y && position.y < chunkPosition.y + CHUNK_SIZE &&
                                            position.z >= chunkPosition.z && position.z < chunkPosition.z + CHUNK_SIZE) {
                                            chunkInstanceData.push_back({glm::vec4(position.x, position.y, position.z, 1.0f), color});
                                        }
                                    }
                                }
                            }
                        }

                        if (!chunkInstanceData.empty()) {
                            Chunk* newChunk = new Chunk(arxDevice, chunkPosition, voxel, chunkInstanceData);
                            m_vpChunks.push_back(newChunk);
                            if (newChunk->getID() != -1) {
                                setChunkPosition({chunkPosition, newChunk->getID()});
                                setChunkAABB(newChunk->getPosition(), newChunk->getID());
                            }
                        }
                    }
                }
            }
            ogt_vox_destroy_scene(scene);
        }
    }

    void ChunkManager::setChunkPosition(const std::pair<glm::vec3, unsigned int>& position) {
            chunkPositions.push_back({position.first, position.second});
    }

    void ChunkManager::setChunkAABB(const glm::vec3& position, const unsigned int chunkId) {
        
        AABB aabb;
        aabb.min = position;
        aabb.max = position + glm::vec3(ADJUSTED_CHUNK);

        chunkAABBs[chunkId] = aabb;
    }
}
