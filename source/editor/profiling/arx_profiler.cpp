#include "../source/editor/profiling/arx_profiler.hpp"
#include "../source/editor/logging/arx_logger.hpp"

#include <algorithm>
#include <stdexcept>

namespace arx {

    std::chrono::high_resolution_clock::time_point Timer::start_time;

    void Timer::start() {
        start_time = std::chrono::high_resolution_clock::now();
    }

    double Timer::stop() {
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(end_time - start_time);
        return duration.count();
    }

    std::array<VkQueryPool, Profiler::BUFFER_COUNT> Profiler::queryPools = {VK_NULL_HANDLE, VK_NULL_HANDLE};
    std::array<std::vector<std::pair<std::string, uint32_t>>, Profiler::BUFFER_COUNT> Profiler::activeQueries;
    uint32_t Profiler::currentBufferIndex = 0;
    uint32_t Profiler::queryCount = 1000;
    uint32_t Profiler::frameCounter = 0;
    ArxDevice* Profiler::device = nullptr;

    std::vector<std::pair<std::string, std::chrono::high_resolution_clock::time_point>> Profiler::cpuActiveTimers;
    std::vector<std::pair<std::string, double>> Profiler::cpuStageDurations;

    void Profiler::initializeGPUProfiler(ArxDevice& dev, uint32_t maxQueries) {
        device = &dev;
        queryCount = maxQueries;

        for (int i = 0; i < BUFFER_COUNT; i++) {
            VkQueryPoolCreateInfo queryPoolInfo{};
            queryPoolInfo.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
            queryPoolInfo.queryType = VK_QUERY_TYPE_TIMESTAMP;
            queryPoolInfo.queryCount = queryCount;

            if (vkCreateQueryPool(device->device(), &queryPoolInfo, nullptr, &queryPools[i]) != VK_SUCCESS) {
                ARX_LOG_ERROR("Failed to create query pool!");
            }
        }
    }

    void Profiler::startFrame(VkCommandBuffer cmdBuffer) {
        frameCounter++;
        currentBufferIndex = frameCounter % BUFFER_COUNT;
        vkCmdResetQueryPool(cmdBuffer, queryPools[currentBufferIndex], 0, queryCount);
        activeQueries[currentBufferIndex].clear();
    }

    void Profiler::startStageTimer(const std::string& stageName, Type type, VkCommandBuffer cmdBuffer) {
        if (type == Type::CPU) {
            Timer::start();
            cpuActiveTimers.emplace_back(stageName, Timer::start_time);
        } else if (type == Type::GPU && cmdBuffer != VK_NULL_HANDLE) {
            uint32_t queryIndex = static_cast<uint32_t>(activeQueries[currentBufferIndex].size()) * 2;
            if (queryIndex >= queryCount) {
                ARX_LOG_ERROR("Exceeded maximum number of GPU queries!");
                return;
            }
            activeQueries[currentBufferIndex].emplace_back(stageName, queryIndex);
            vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPools[currentBufferIndex], queryIndex);
        }
    }

    void Profiler::stopStageTimer(const std::string& stageName, Type type, VkCommandBuffer cmdBuffer) {
        if (type == Type::CPU) {
            auto it = std::find_if(cpuActiveTimers.begin(), cpuActiveTimers.end(),
                [&stageName](const auto& pair) { return pair.first == stageName; });

            if (it == cpuActiveTimers.end()) {
                ARX_LOG_ERROR("Attempted to stop CPU timer {} that wasn't started!", stageName);
                return;
            }

            double duration = Timer::stop();
            cpuStageDurations.emplace_back(stageName, duration);
            cpuActiveTimers.erase(it);
        } else if (type == Type::GPU && cmdBuffer != VK_NULL_HANDLE) {
            auto it = std::find_if(activeQueries[currentBufferIndex].begin(), activeQueries[currentBufferIndex].end(),
            [&stageName](const auto& pair) { return pair.first == stageName; });

            if (it == activeQueries[currentBufferIndex].end()) {
                ARX_LOG_ERROR("Attempted to stop GPU timer '{}' that wasn't started in buffer {}!", stageName, currentBufferIndex);
                return;
            }

            uint32_t queryIndex = it->second + 1;
            vkCmdWriteTimestamp(cmdBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPools[currentBufferIndex], queryIndex);
        }
    }

    std::vector<std::pair<std::string, double>> Profiler::getProfileData(uint32_t frameOffset) {
        uint32_t bufferIndex = (frameCounter + BUFFER_COUNT - frameOffset) % BUFFER_COUNT;

        std::vector<uint64_t> queryResults(queryCount);
        
        VkResult result = vkGetQueryPoolResults(device->device(), queryPools[bufferIndex], 0, queryCount,
            queryResults.size() * sizeof(uint64_t), queryResults.data(), sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);

        if (result != VK_SUCCESS) {
            ARX_LOG_ERROR("Failed to get query pool results for buffer {}. Error code: {}", bufferIndex, result);
            return {};
        }

        std::vector<std::pair<std::string, double>> stageDurations;
        for (const auto& query : activeQueries[bufferIndex]) {
            uint64_t startTime = queryResults[query.second];
            uint64_t endTime = queryResults[query.second + 1];
            
            if (startTime == 0 || endTime == 0) {
                ARX_LOG_WARNING("Invalid timestamp for stage '{}' in buffer {}. Start: {}, End: {}", 
                                query.first, bufferIndex, startTime, endTime);
                continue;
            }

            double duration = static_cast<double>(endTime - startTime) / 1e6;// * device->getProperties().limits.timestampPeriod;
            stageDurations.emplace_back(query.first, duration);
        }

        return stageDurations;
    }

    void Profiler::cleanup(ArxDevice& device) {
        for (auto queryPool : queryPools) {
            if (queryPool != VK_NULL_HANDLE) {
                vkDestroyQueryPool(device.device(), queryPool, nullptr);
            }
        }
    }
}