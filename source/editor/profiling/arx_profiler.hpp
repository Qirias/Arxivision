#pragma once

#include "../source/arx_device.h"
#include <chrono>
#include <string>
#include <vector>
#include <array>
#include <vulkan/vulkan.h>

namespace arx {

    class Timer {
    public:
        static void start();
        static double stop();

        static std::chrono::high_resolution_clock::time_point start_time;
    };

    class Profiler {
    public:
        enum class Type {
            CPU,
            GPU
        };

        static void initializeGPUProfiler(ArxDevice& device, uint32_t maxQueries = 1000);
        static void startFrame(VkCommandBuffer cmdBuffer);
        static void startStageTimer(const std::string& stageName, Type type = Type::CPU, VkCommandBuffer cmdBuffer = VK_NULL_HANDLE);
        static void stopStageTimer(const std::string& stageName, Type type = Type::CPU, VkCommandBuffer cmdBuffer = VK_NULL_HANDLE);
        static std::vector<std::pair<std::string, double>> getProfileData(uint32_t frameOffset = 1);
        static void cleanup(ArxDevice& device);

        static void debugPrintState();
    private:
        static const int BUFFER_COUNT = 2;
        static std::array<VkQueryPool, BUFFER_COUNT> queryPools;
        static std::array<std::vector<std::pair<std::string, uint32_t>>, BUFFER_COUNT> activeQueries;
        static uint32_t currentBufferIndex;
        static uint32_t queryCount;
        static ArxDevice* device;

        static uint32_t frameCounter;

        static std::vector<std::pair<std::string, std::chrono::high_resolution_clock::time_point>> cpuActiveTimers;
        static std::vector<std::pair<std::string, double>> cpuStageDurations;
    };
}