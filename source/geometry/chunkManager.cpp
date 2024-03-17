#include "chunkManager.h"

namespace arx {
    
    ChunkManager::ChunkManager(ArxDevice &device) : arxDevice(device), m_IsUpdateThreadRunning(true) {
        
    }

    ChunkManager::~ChunkManager() {
        // Stop the update thread
        m_IsUpdateThreadRunning = false;
        
        // Clean up resources
        for (Chunk* chunk : m_vpChunks)
            delete chunk;

        // No need to manually free command buffers and destroy command pools here
    }

    void ChunkManager::StartUpdateThread() {
        std::call_once(m_ThreadStartFlag, [this]() {
            arxDevice.threadPool.threads[0]->addJob([this]() {
                UpdateThreadFunction();
            });
        });
    }

    void ChunkManager::UpdateThreadFunction() {
        while (m_IsUpdateThreadRunning) {
            ArxGameObject::Map* localGameObjectsPtr = nullptr;
            glm::vec3 localPlayerPosition;

            {
//                std::unique_lock<std::mutex> lock(updateMutex);
                // Wait for initialization
//                initializationCV.wait(lock, [this]() { return isInitialized; });

                // Copy the shared data to local variables
                localGameObjectsPtr = gameObjectsPtr;
                localPlayerPosition = playerPosition;
            } // lock's destructor automatically unlocks the mutex when it goes out of scope

            if (localGameObjectsPtr) {
                ArxGameObject::Map& gameObjects = *localGameObjectsPtr;
                Update(gameObjects, localPlayerPosition); // Call Update function with local data
            }

            // Sleep for a short time to avoid busy waiting
//            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }

    void ChunkManager::UpdateGameObjectsAndCamera(ArxGameObject::Map& updatedGameObjects, const glm::vec3& playerPos) {
        gameObjectsPtr = &updatedGameObjects;
        playerPosition = playerPos;
        isInitialized = true;
    }

    void ChunkManager::Update(ArxGameObject::Map& gameObjects, const glm::vec3 &playerPosition) {
        // Determine the minimum and maximum chunk indices based on the player's position
        const float chunkCreationThreshold = 100.0f;
        const float exclusionDistance = chunkCreationThreshold + CHUNK_SIZE;
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
                    std::cout << m_vpChunks.size() << std::endl;
                }
            }
        }

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
        // Setup the chunk and add it to the appropriate lists
        // (e.g., m_vpChunkLoadList, m_vpChunkSetupList, etc.)
        // based on your existing implementation
        return newChunk;
    }
}
