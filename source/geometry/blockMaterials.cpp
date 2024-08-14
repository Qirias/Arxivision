#include "blockMaterials.hpp"

namespace arx {

    std::unique_ptr<ArxBuffer> Materials::pointLightBuffer;
    std::unordered_map<uint32_t, ChunkLightInfo> Materials::chunkLightInfos;
    std::vector<PointLight> Materials::pointLightsCPU;
    uint32_t Materials::maxPointLights = 0;
    uint32_t Materials::currentPointLightCount = 0;

    void Materials::initialize(ArxDevice& device, std::unordered_map<uint32_t, std::vector<PointLight>>& chunkLights) {
        
        // Count total lights
        maxPointLights = 0;
        for (const auto& [chunkID, lights] : chunkLights) {
            maxPointLights += lights.size();
        }

        // Create buffer
        VkDeviceSize bufferSize = sizeof(PointLight) * maxPointLights;
        pointLightBuffer = std::make_unique<ArxBuffer>(
            device,
            bufferSize,
            maxPointLights,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            device.properties.limits.minStorageBufferOffsetAlignment
        );
        pointLightBuffer->map();

        // Fill buffer and set up chunkLightInfos
        uint32_t offset = 0;
        for (auto& [chunkID, lights] : chunkLights) {
            ChunkLightInfo cli;
            cli.offset = offset;
            cli.count = static_cast<uint32_t>(lights.size());
            chunkLightInfos[chunkID] = cli;

            pointLightBuffer->writeToBuffer(lights.data(), lights.size() * sizeof(PointLight), offset * sizeof(PointLight));
            offset += lights.size();
        }

        currentPointLightCount = maxPointLights;
    }

    // Untested
    void Materials::addPointLightToChunk(ArxDevice& device, uint32_t chunkID, PointLight& pl) {
        if (currentPointLightCount >= maxPointLights) {
            resizeBuffer(device, maxPointLights * 2);
        }

        auto& cli = chunkLightInfos[chunkID];
        uint32_t insertOffset = cli.offset + cli.count;

        // Shift lights after the insertion point
        for (auto& [id, info] : chunkLightInfos) {
            if (info.offset > insertOffset) {
                shiftLights(info.offset, info.count, 1);
                info.offset++;
            }
        }

        // Insert new light
        pointLightBuffer->writeToBuffer(&pl, sizeof(PointLight), insertOffset * sizeof(PointLight));

        cli.count++;
        currentPointLightCount++;
    }

    // Untested
    void Materials::removePointLightFromChunk(uint32_t chunkID, const glm::vec3& lightPosition) {
        auto it = chunkLightInfos.find(chunkID);
        if (it != chunkLightInfos.end()) {
            auto& cli = it->second;
            
            std::vector<PointLight> chunkLights(cli.count);
            pointLightBuffer->readFromBuffer(chunkLights.data(), cli.count * sizeof(PointLight), cli.offset * sizeof(PointLight));

            auto lightIt = std::find_if(chunkLights.begin(), chunkLights.end(),
                [&lightPosition](const PointLight& light) {
                    return glm::distance(light.position, lightPosition) < 0.001f;
                });

            if (lightIt != chunkLights.end()) {
                uint32_t removeIndex = static_cast<uint32_t>(cli.offset + std::distance(chunkLights.begin(), lightIt));

                // Shift lights after the removal point
                for (auto& [id, info] : chunkLightInfos) {
                    if (info.offset > removeIndex) {
                        shiftLights(info.offset, info.count, -1);
                        info.offset--;
                    }
                }

                cli.count--;
                currentPointLightCount--;

                if (cli.count == 0) {
                    chunkLightInfos.erase(it);
                }
            }
        }
    }

    void Materials::resizeBuffer(ArxDevice& device, uint32_t newSize) {
        VkDeviceSize newBufferSize = sizeof(PointLight) * newSize;

        auto newBuffer = std::make_unique<ArxBuffer>(
            device,
            newBufferSize,
            newSize,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        newBuffer->map();

        // Copy existing data to new buffer
        VkDeviceSize copySize = sizeof(PointLight) * currentPointLightCount;
        void* existingData = nullptr;
        pointLightBuffer->readFromBuffer(&existingData, copySize, 0);
        newBuffer->writeToBuffer(existingData, copySize, 0);

        // Swap buffers
        pointLightBuffer = std::move(newBuffer);
        maxPointLights = newSize;
    }

    void Materials::shiftLights(uint32_t startOffset, uint32_t count, int32_t shiftAmount) {
        std::vector<PointLight> lights(count);
        pointLightBuffer->readFromBuffer(lights.data(), count * sizeof(PointLight), startOffset * sizeof(PointLight));
        pointLightBuffer->writeToBuffer(lights.data(), count * sizeof(PointLight), (startOffset + shiftAmount) * sizeof(PointLight));
    }

    // Untested
    void Materials::updateBufferForChunk(uint32_t chunkID) {
        auto it = chunkLightInfos.find(chunkID);
        if (it != chunkLightInfos.end()) {
            auto& cli = it->second;
            VkDeviceSize offset = cli.offset * sizeof(PointLight);
            VkDeviceSize size = cli.count * sizeof(PointLight);
            void* dataPtr = pointLightsCPU.data() + cli.offset;
            pointLightBuffer->writeToBuffer(dataPtr, size, offset);
        }
    }

    void Materials::printLights() {
        std::cout << "==== Point Lights ====\n";
        std::cout << "Total lights: " << currentPointLightCount << "\n\n";

        for (const auto& [chunkID, cli] : chunkLightInfos) {
            std::cout << "Chunk ID: " << chunkID << "\n";
            std::cout << "Light count: " << cli.count << "\n";

            std::vector<PointLight> chunkLights(cli.count);
            pointLightBuffer->readFromBuffer(chunkLights.data(), cli.count * sizeof(PointLight), cli.offset * sizeof(PointLight));

            for (size_t i = 0; i < chunkLights.size(); ++i) {
                const auto& light = chunkLights[i];
                std::cout << "  Light " << i << ":\n";
                std::cout << "    Position: (" << light.position.x << ", "
                                               << light.position.y << ", "
                                               << light.position.z << ")\n";
                std::cout << "    Color: (" << light.color.r << ", "
                                            << light.color.g << ", "
                                            << light.color.b << ")\n";
            }
            std::cout << "\n";
        }
        std::cout << "=====================\n";
    }


    void Materials::cleanup() {
        pointLightBuffer->unmap();
        pointLightBuffer.reset();
        pointLightsCPU.clear();
        chunkLightInfos.clear();
        currentPointLightCount = 0;
    }

}
