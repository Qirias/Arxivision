#pragma once

#include "arx_window.h"

// std lib headers
#include <string>
#include <vector>
#include "threadpool.h"

namespace arx {

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

struct QueueFamilyIndices {
    uint32_t graphicsFamily;
    uint32_t presentFamily;
    bool graphicsFamilyHasValue = false;
    bool presentFamilyHasValue = false;
    bool isComplete() { return graphicsFamilyHasValue && presentFamilyHasValue; }
};

class ArxDevice {

    public:
    #ifdef NDEBUG
        const bool enableValidationLayers = false;
    #else
        const bool enableValidationLayers = true;
    #endif

    ArxDevice(ArxWindow &window);
    ~ArxDevice();

    // Not copyable or movable
    ArxDevice(const ArxDevice &) = delete;
    ArxDevice& operator=(const ArxDevice &) = delete;
    ArxDevice(ArxDevice &&) = delete;
    ArxDevice &operator=(ArxDevice &&) = delete;

    VkCommandPool getCommandPool() { return commandPool; }
    VkDevice device() { return _device; }
    VkSurfaceKHR surface() { return _surface; }
    VkQueue graphicsQueue() { return _graphicsQueue; }
    VkQueue presentQueue() { return _presentQueue; }


    SwapChainSupportDetails getSwapChainSupport() { return querySwapChainSupport(physicalDevice); }
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties);
    QueueFamilyIndices findPhysicalQueueFamilies() { return findQueueFamilies(physicalDevice); }
    VkFormat findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features);

    // Buffer Helper Functions
    void createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer &buffer,
        VkDeviceMemory &bufferMemory);
    VkCommandBuffer beginSingleTimeCommands();
    void endSingleTimeCommands(VkCommandBuffer commandBuffer);
    void copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size, VkDeviceSize srcOffset = 0, VkDeviceSize dstOffset = 0);
    void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount);
    void destroyBuffer(VkBuffer buffer);
    void freeMemory(VkDeviceMemory memory);

    void createImageWithInfo(
        const VkImageCreateInfo &imageInfo,
        VkMemoryPropertyFlags properties,
        VkImage &image,
        VkDeviceMemory &imageMemory);

    VkPhysicalDeviceProperties properties;
    VkSampleCountFlagBits msaaSamples = VK_SAMPLE_COUNT_1_BIT;

    VkInstance getInstance() const { return instance; }
    VkPhysicalDevice getPhysicalDevice() const { return physicalDevice; }
    
    private:
    void createInstance();
    void setupDebugMessenger();
    void createSurface();
    void pickPhysicalDevice();
    void setImagelessFramebufferFeature();
    void setBufferDeviceAddressFeature();
    void createLogicalDevice();
    void createCommandPool();

    // helper functions
    bool isDeviceSuitable(VkPhysicalDevice device);
    std::vector<const char *> getRequiredExtensions();
    bool checkValidationLayerSupport();
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo);
    void hasGflwRequiredInstanceExtensions();
    bool checkDeviceExtensionSupport(VkPhysicalDevice device);
    SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
    VkSampleCountFlagBits getMaxUsableSampleCount();

    VkInstance                                      instance;
    VkDebugUtilsMessengerEXT                        debugMessenger;
    VkPhysicalDevice                                physicalDevice = VK_NULL_HANDLE;
    ArxWindow                                       &window;
    VkCommandPool                                   commandPool;
    VkPhysicalDeviceImagelessFramebufferFeatures    imagelessFramebufferFeatures;
    VkPhysicalDeviceBufferDeviceAddressFeatures     bufferDeviceAddressFeatures;
    bool                                            supportsBufferDeviceAddress;
    
    uint32_t                                        numThreads{0};

    VkDevice      _device;
    VkSurfaceKHR  _surface;
    VkQueue       _graphicsQueue;
    VkQueue       _presentQueue;

    const std::vector<const char *> validationLayers = {"VK_LAYER_KHRONOS_validation"};
    const std::vector<const char *> deviceExtensions = {VK_KHR_SWAPCHAIN_EXTENSION_NAME,
                                                        VK_KHR_SHADER_DRAW_PARAMETERS_EXTENSION_NAME,
                                                        "VK_KHR_portability_subset",
                                                        "VK_KHR_imageless_framebuffer",
                                                        "VK_KHR_maintenance2",
                                                        "VK_KHR_image_format_list"};
        
    public:
        ThreadPool                                      threadPool;
    };
}
