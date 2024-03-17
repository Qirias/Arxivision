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

        void StartUpdateThread();
        void UpdateGameObjectsAndCamera(ArxGameObject::Map& updatedGameObjects, const glm::vec3& playerPos);
        const std::vector<Chunk*>& GetChunks() const { return m_vpChunks; }

    private:
        ArxDevice& arxDevice;
        std::vector<Chunk*> m_vpChunks;
    
        std::mutex updateMutex;
        std::condition_variable initializationCV;
        ArxGameObject::Map* gameObjectsPtr = nullptr;
        glm::vec3 playerPosition;
        bool isInitialized = false;
        bool m_IsUpdateThreadRunning;
        std::once_flag m_ThreadStartFlag;

        void Update(ArxGameObject::Map& gameObjects, const glm::vec3 &playerPosition);
        bool HasChunkAtPosition(const glm::vec3& position) const;
        bool IsChunkAtPosition(const glm::vec3& position) const;
        Chunk* CreateChunk(ArxGameObject::Map& gameObjects, const glm::vec3 &position);
        void UpdateThreadFunction(); // Not used anymore
    };
}
