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
                    Chunk* newChunk = new Chunk(arxDevice, chunkPosition, voxel, chunkVertices);
                    m_vpChunks.push_back(newChunk);
                    setChunkPosition({chunkPosition, newChunk->getID()});
                }
            }
        }
    }

    void ChunkManager::diamondSquare(const glm::ivec2& size) {
        heightMap.resize(size.x, std::vector<float>(size.y, 0.0f));

        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_real_distribution<> dis(-40, 150);

        // Initialize corners with values closer to maxHeight
        heightMap[0][0] = dis(gen);
        heightMap[0][size.y - 1] = dis(gen);
        heightMap[size.x - 1][0] = dis(gen);
        heightMap[size.x - 1][size.y - 1] = dis(gen);

        int sideLength = size.x - 1;
        float roughness = 0.3f;
        
        while (sideLength >= 2) {
            int halfSide = sideLength / 2;

            // Diamond step
            for (int x = 0; x < size.x - 1; x += halfSide) {
                for (int y = 0; y < size.y - 1; y += halfSide) {
                    int midX = x + halfSide;
                    int midY = y + halfSide;
                    float avg = (
                        heightMap[x][y] +
                        heightMap[(x + sideLength) % size.x][y] +
                        heightMap[x][(y + sideLength) % size.y] +
                        heightMap[(x + sideLength) % size.x][(y + sideLength) % size.y]
                    ) / 4.0f;
                    heightMap[midX % size.x][midY % size.y] = avg + dis(gen) * roughness;
                }
            }

            // Square step
            for (int x = 0; x < size.x; x += halfSide) {
                for (int y = (x + halfSide) % sideLength; y < size.y; y += sideLength) {
                    float sum = 0.0f;
                    int count = 0;

                    // Collect the four surrounding heights
                    if (x >= halfSide) { sum += heightMap[x - halfSide][y]; count++; }
                    if (x + halfSide < size.x) { sum += heightMap[(x + halfSide) % size.x][y]; count++; }
                    if (y >= halfSide) { sum += heightMap[x][y - halfSide]; count++; }
                    if (y + halfSide < size.y) { sum += heightMap[x][(y + halfSide) % size.y]; count++; }

                    float avg = sum / count;
                    heightMap[x][y] = avg + dis(gen) * roughness;
                }
            }
            
            sideLength = halfSide;
            roughness *= std::pow(2.0, -0.5); // Decrease roughness
        }
    }


    void ChunkManager::initializeHeightTerrain(ArxGameObject::Map& voxel, int n) {
        glm::ivec3 terrainSize = glm::ivec3(n * ADJUSTED_CHUNK, n * ADJUSTED_CHUNK, n * ADJUSTED_CHUNK);
        glm::ivec2 heightMapSize = glm::ivec2(terrainSize.x, terrainSize.z);
        diamondSquare(heightMapSize);

        for (int chunkX = 0; chunkX < n; chunkX++) {
            for (int chunkY = 0; chunkY < n; chunkY++) {
                for (int chunkZ = 0; chunkZ < n; chunkZ++) {
                    glm::vec3 chunkPosition = glm::vec3(chunkX * ADJUSTED_CHUNK, chunkY * ADJUSTED_CHUNK, chunkZ * ADJUSTED_CHUNK);
                    glm::ivec3 globalOffset(chunkX * ADJUSTED_CHUNK, chunkY * ADJUSTED_CHUNK, chunkZ * ADJUSTED_CHUNK);
                    Chunk* newChunk = new Chunk(arxDevice, chunkPosition, voxel, heightMap, globalOffset);
                    m_vpChunks.push_back(newChunk);
                    setChunkPosition({chunkPosition, newChunk->getID()});
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
                    setChunkPosition({newChunk->getPosition(), newChunk->getID()});
                }
            }
        }
    }


    
    void ChunkManager::setChunkPosition(const std::pair<glm::vec3, unsigned int>& position) {
        chunkPositions.push_back({position.first, position.second});
    }
}
