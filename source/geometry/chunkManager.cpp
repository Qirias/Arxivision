#include "chunkManager.h"

namespace arx {
    
    ChunkManager::ChunkManager(ArxDevice &device) : arxDevice{device} {
        
    }

    ChunkManager::~ChunkManager() {
        for (Chunk* chunk : m_vpChunks)
            delete chunk;
    }

    void ChunkManager::processBuilder(ArxGameObject::Map& voxel) {
        
        float scaleFactor = 1.0f;
        
        builder.loadModel("models/obj_1.obj");

        glm::mat4 viewMatrix = camera.getView();
        glm::mat4 projectionMatrix = camera.getProjection();

        glm::vec3 minBounds = glm::vec3(std::numeric_limits<float>::max());
        glm::vec3 maxBounds = glm::vec3(std::numeric_limits<float>::lowest());

        // Transform vertices and normals into world space
        for (int i = 0; i < builder.vertices.size(); i++) {
            // Assuming vertex.position and vertex.normal are in local space
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

        // Calculate scaled model size
        glm::vec3 modelSize = maxBounds - minBounds;

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
                    Chunk* newChunk = CreateChunk(voxel, chunkPosition, chunkVertices);
                    m_vpChunks.push_back(newChunk);
                }
            }
        }
    }


    void ChunkManager::Update(ArxGameObject::Map& voxel, const glm::vec3 &playerPosition) {
    }


    bool ChunkManager::HasChunkAtPosition(const glm::vec3& position) const {
        // Calculate the minimum and maximum positions within the area
        glm::vec3 minPosition = glm::floor(position);
        glm::vec3 maxPosition = glm::ceil(position);

        // Iterate over all points within the area
        for (float x = minPosition.x; x <= maxPosition.x; x += CHUNK_SIZE) {
            for (float y = minPosition.y; y <= maxPosition.y; y += CHUNK_SIZE) {
                for (float z = minPosition.z; z <= maxPosition.z; z += CHUNK_SIZE) {
                    glm::vec3 chunkPosition(x, y, z);

                    // Check if there is a chunk at the current position
                    if (IsChunkAtPosition(chunkPosition)) {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    bool ChunkManager::IsChunkAtPosition(const glm::vec3& position) const {
        for (Chunk* chunk : m_vpChunks) {
            glm::vec3 chunkPosition = chunk->getPosition();

            if (chunkPosition == position) {
                return true;
            }
        }
        
        return false;
    }

    Chunk* ChunkManager::CreateChunk(ArxGameObject::Map& voxel, const glm::vec3 &position, std::vector<ArxModel::Vertex>& vertices) {
        Chunk* newChunk = new Chunk(arxDevice, position, voxel, vertices);
        
        return newChunk;
    }
}
