#include "chunkManager.h"

namespace arx {
    
    ChunkManager::ChunkManager(ArxDevice &device) : arxDevice{device} {
        
    }

    ChunkManager::~ChunkManager() {
        
    }

    void ChunkManager::obj2vox(ArxGameObject::Map& voxel, const std::string& path, const float scaleFactor) {
        builder.loadModel(path);

        glm::dmat4 viewMatrix = camera.getView();
        glm::dmat4 projectionMatrix = camera.getProjection();

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
                    Chunk* newChunk = CreateChunk(voxel, chunkPosition, chunkVertices);
                    m_vpChunks.push_back(newChunk);
                }
            }
        }
    }

    void ChunkManager::initializeTerrain(ArxGameObject::Map& voxel, const glm::ivec3& terrainSize) {
        this->terrainSize = terrainSize;
        
        int numChunksX = (terrainSize.x + CHUNK_SIZE ) / CHUNK_SIZE;
        int numChunksY = (terrainSize.y + CHUNK_SIZE ) / CHUNK_SIZE;
        int numChunksZ = (terrainSize.z + CHUNK_SIZE ) / CHUNK_SIZE;

        // This vector represents an empty placeholder since we're not using model vertices for initialization.
        std::vector<arx::ArxModel::Vertex> emptyVertices;

        for (int x = 0; x < numChunksX; ++x) {
            for (int y = 0; y < numChunksY; ++y) {
                for (int z = 0; z < numChunksZ; ++z) {
                    glm::vec3 chunkPosition(x * CHUNK_SIZE, y * CHUNK_SIZE, z * CHUNK_SIZE);
                    Chunk* newChunk = CreateChunk(voxel, chunkPosition, emptyVertices);
                    m_vpChunks.push_back(newChunk);
                }
            }
        }
    }

    Chunk* ChunkManager::CreateChunk(ArxGameObject::Map& voxel, const glm::vec3 &position, std::vector<ArxModel::Vertex>& vertices) {
        Chunk* newChunk = new Chunk(arxDevice, position, voxel, vertices);
        
        return newChunk;
    }
}
