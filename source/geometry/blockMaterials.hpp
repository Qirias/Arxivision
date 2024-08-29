#pragma once

#include "../source/arx_buffer.h"


namespace arx {

    struct PointLight {
        glm::vec3 position;
        uint32_t visibilityMask;
        glm::vec4 color;
    };

    struct ChunkLightInfo {
        uint32_t offset; // Offset in the buffer (in terms of PointLight instances)
        uint32_t count;
    };

    class Materials {
    public:
                
        static void addPointLight(const PointLight& pl);
        static void removePointLight(const glm::vec3 keyPos);
        
        static void initialize(ArxDevice& device, std::unordered_map<uint32_t, std::vector<PointLight>>& chunkLights);
        static void addPointLightToChunk(ArxDevice& device, uint32_t chunkID, PointLight& pl);
        static void removePointLightFromChunk(uint32_t chunkID, const glm::vec3& lightPosition);

        static void updateBufferForChunk(uint32_t chunkID);
        static void cleanup();
        static void printLights();
        
        // Buffer to store all point lights
        static std::shared_ptr<ArxBuffer> pointLightBuffer;

        // Mapping from chunkID to ChunkLightInfo
        static std::unordered_map<uint32_t, ChunkLightInfo> chunkLightInfos;

        // Staging area in CPU
        static std::vector<PointLight> pointLightsCPU;

        static uint32_t maxPointLights;
        static uint32_t currentPointLightCount;
        
    private:
        static void resizeBuffer(ArxDevice& device, uint32_t newSize);
        static void shiftLights(uint32_t startOffset, uint32_t count, int32_t shiftAmount);
        
        glm::vec3 revertChunkID(uint32_t chunkID, uint32_t &chunkX, uint32_t &chunkY, uint32_t &chunkZ) {
            chunkX = chunkID & 0x3FF;
            chunkY = (chunkID >> 10) & 0x3FF;
            chunkZ = (chunkID >> 20) & 0x3FF;
            
            return glm::vec3(chunkX, chunkY, chunkZ);
        }
    };
}
