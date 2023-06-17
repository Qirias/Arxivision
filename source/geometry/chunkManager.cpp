#include "chunkManager.h"

namespace arx {
    
    ChunkManager::ChunkManager(ArxDevice &device) : arxDevice{device} {
        
    }

    ChunkManager::~ChunkManager() {
        for (Chunk* chunk : m_vpChunks)
            delete chunk;
    }


    void ChunkManager::Update(ArxGameObject::Map& gameObjects, const glm::vec3 &playerPosition) {
        // TODO: Check if the player is close to an area with no existing chunks
        // and create a new chunk if necessary

        const float chunkCreationThreshold = 10.0f;

        const float exclusionDistance = chunkCreationThreshold + CHUNK_SIZE;

        // Determine the minimum and maximum chunk indices based on the player's position
        float minX = std::floor((playerPosition.x - exclusionDistance) / CHUNK_SIZE);
        float minZ = std::floor((playerPosition.z - exclusionDistance) / CHUNK_SIZE);
        float maxX = std::ceil((playerPosition.x + exclusionDistance) / CHUNK_SIZE);
        float maxZ = std::ceil((playerPosition.z + exclusionDistance) / CHUNK_SIZE);

        int y = 0; // Specify the desired Y level

        // Generate chunks within the specified range on the specified Y level
        for (float x = minX; x <= maxX; ++x) {
            for (float z = minZ; z <= maxZ; ++z) {
                glm::vec3 chunkPosition(x * CHUNK_SIZE, y * CHUNK_SIZE, z * CHUNK_SIZE);

                // Check if a chunk already exists at this position
                if (!HasChunkAtPosition(chunkPosition)) {
                    // Create a new chunk at the position
                    Chunk* newChunk = CreateChunk(gameObjects, chunkPosition);
                    m_vpChunks.push_back(newChunk);
                    std::cout << "Chunks: " << m_vpChunks.size() << std::endl;
                    std::cout << "Chunk pos: " << chunkPosition.x << "," << chunkPosition.y << "," << chunkPosition.z << std::endl;
                }
            }
        }

        // TODO: Update other game logic and chunk management...
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
            // Get the position of the chunk
            glm::vec3 chunkPosition = chunk->getPosition();

            // Check if the chunk position matches the specified position
            if (chunkPosition == position) {
                return true;
            }
        }
        
        return false;
    }

    Chunk* ChunkManager::CreateChunk(ArxGameObject::Map& gameObjects, const glm::vec3 &position) {
        Chunk* newChunk = new Chunk(arxDevice, position, gameObjects);
        
        // TODO
        // Setup the chunk and add it to the appropriate lists
        // (e.g., m_vpChunkLoadList, m_vpChunkSetupList, etc.)
        // based on your existing implementation
        // ...
        
        return newChunk;
    }
}
