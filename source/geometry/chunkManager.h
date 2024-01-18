#pragma once

#include "chunks.h"
#include "arx_pipeline.h"
#include "arx_frame_info.h"

#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <thread>
#include <mutex>

namespace arx {

    class ChunkManager {
    public:
        ChunkManager(ArxDevice &device);
        ~ChunkManager();
        
        void Update(ArxGameObject::Map& gameObjects, const glm::vec3& playerPosition);
        void UpdateGameObjectsAndCamera(ArxGameObject::Map& updatedGameObjects, const glm::vec3& cameraPosition);
        void StartUpdateThread();

        Chunk* CreateChunk(ArxGameObject::Map& gameObjects, const glm::vec3& position);
        bool IsPointInArea(const glm::vec3& point, const glm::vec3& areaCenter, float areaSize) const;
        glm::vec3 CalculateAdjacentChunkPosition(const glm::vec3& position) const;
        const std::vector<Chunk*>& GetChunks() const { return m_vpChunks; }
        bool IsChunkAtPosition(const glm::vec3& position) const;
        
    private:
        ArxDevice           &arxDevice;
        std::vector<Chunk*> m_vpChunks;
        
        bool HasChunkAtPosition(const glm::vec3& position) const;
        
        std::thread             m_UpdateThread;
        std::atomic<bool>       m_IsUpdateThreadRunning;
        std::mutex              updateMutex; // Declare and initialize the mutex
        ArxGameObject::Map*     gameObjectsPtr; // Shared reference to updated gameObjects
        glm::vec3               playerPosition; // Shared updated camera position
        std::condition_variable initializationCV;
        bool                    isInitialized = false;

        void UpdateThreadFunction();
    };
}
