#include "arx_device.h"

// std headers
#include <cstring>
#include <iostream>
#include <set>
#include <unordered_set>

namespace arx {

        // local callback functions
        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
        void *pUserData) {
            std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

            return VK_FALSE;
        }

        VkResult CreateDebugUtilsMessengerEXT(
        VkInstance instance,
        const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
        const VkAllocationCallbacks *pAllocator,
        VkDebugUtilsMessengerEXT *pDebugMessenger) {
            auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
              instance,
              "vkCreateDebugUtilsMessengerEXT");
            if (func != nullptr) {
                return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
            } else {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }
        }

        void DestroyDebugUtilsMessengerEXT(
        VkInstance instance,
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks *pAllocator) {
            auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
              instance,
              "vkDestroyDebugUtilsMessengerEXT");
            if (func != nullptr) {
            func(instance, debugMessenger, pAllocator);
            }
        }

        // class member functions
        ArxDevice::ArxDevice(ArxWindow &window) : window{window} {
            createInstance();
            setupDebugMessenger();
            createSurface();
            pickPhysicalDevice();
            createLogicalDevice();
            createCommandPool();
        }

        ArxDevice::~ArxDevice() {
            vkDestroyCommandPool(_device, commandPool, nullptr);
            vkDestroyDevice(_device, nullptr);

            if (enableValidationLayers) {
                DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
            }

            vkDestroySurfaceKHR(instance, _surface, nullptr);
            vkDestroyInstance(instance, nullptr);
        }

        void ArxDevice::createInstance() {
            if (enableValidationLayers && !checkValidationLayerSupport()) {
                throw std::runtime_error("validation layers requested, but not available!");
            }

            VkApplicationInfo appInfo     = {};
            appInfo.sType                 = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName      = "Voxel App";
            appInfo.applicationVersion    = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName           = "ArXivision";
            appInfo.engineVersion         = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion            = VK_API_VERSION_1_0;

            VkInstanceCreateInfo createInfo = {};
            createInfo.sType              = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo   = &appInfo;
            createInfo.flags              = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;

            auto extensions = getRequiredExtensions();
            createInfo.enabledExtensionCount      = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames    = extensions.data();

            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;
            if (enableValidationLayers) {
                createInfo.enabledLayerCount    = static_cast<uint32_t>(validationLayers.size());
                createInfo.ppEnabledLayerNames  = validationLayers.data();

            populateDebugMessengerCreateInfo(debugCreateInfo);
            createInfo.pNext                = (VkDebugUtilsMessengerCreateInfoEXT *)&debugCreateInfo;
            } else {
                createInfo.enabledLayerCount = 0;
                createInfo.pNext             = nullptr;
            }

            if (vkCreateInstance(&createInfo, nullptr, &instance) != VK_SUCCESS) {
                throw std::runtime_error("failed to create instance!");
            }

            hasGflwRequiredInstanceExtensions();
        }

        void ArxDevice::pickPhysicalDevice() {
            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
            if (deviceCount == 0) {
                throw std::runtime_error("failed to find GPUs with Vulkan support!");
            }
            std::cout << "Device count: " << deviceCount << std::endl;
            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

            for (const auto &device : devices) {
                if (isDeviceSuitable(device)) {
                  physicalDevice = device;
                  msaaSamples = getMaxUsableSampleCount();
                  break;
                }
            }

            if (physicalDevice == VK_NULL_HANDLE) {
                throw std::runtime_error("failed to find a suitable GPU!");
            }

            vkGetPhysicalDeviceProperties(physicalDevice, &properties);
            std::cout << "physical device: " << properties.deviceName << std::endl;
            std::cout << "maxStorageBufferRange: " << properties.limits.maxStorageBufferRange << std::endl;
            
        }

        void ArxDevice::createLogicalDevice() {
            QueueFamilyIndices indices = findQueueFamilies(physicalDevice);

            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily, indices.presentFamily};

            float queuePriority = 1.0f;
            for (uint32_t queueFamily : uniqueQueueFamilies) {
                VkDeviceQueueCreateInfo queueCreateInfo = {};
                queueCreateInfo.sType               = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex    = queueFamily;
                queueCreateInfo.queueCount          = 1;
                queueCreateInfo.pQueuePriorities    = &queuePriority;
                queueCreateInfos.push_back(queueCreateInfo);
            }

            VkPhysicalDeviceFeatures deviceFeatures = {};
            deviceFeatures.samplerAnisotropy    = VK_TRUE;
            deviceFeatures.fillModeNonSolid     = VK_TRUE;
            
            setImagelessFramebufferFeature();

            VkDeviceCreateInfo createInfo = {};
            createInfo.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;

            createInfo.queueCreateInfoCount    = static_cast<uint32_t>(queueCreateInfos.size());
            createInfo.pQueueCreateInfos       = queueCreateInfos.data();

            createInfo.pEnabledFeatures        = &deviceFeatures;
            createInfo.enabledExtensionCount   = static_cast<uint32_t>(deviceExtensions.size());
            createInfo.ppEnabledExtensionNames = deviceExtensions.data();
            createInfo.pNext                   = &imagelessFramebufferFeatures;

            // might not really be necessary anymore because device specific validation layers
            // have been deprecated
            if (enableValidationLayers) {
                createInfo.enabledLayerCount    = static_cast<uint32_t>(validationLayers.size());
                createInfo.ppEnabledLayerNames  = validationLayers.data();
            } else {
                createInfo.enabledLayerCount    = 0;
            }

            if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &_device) != VK_SUCCESS) {
                throw std::runtime_error("failed to create logical device!");
            }

            vkGetDeviceQueue(_device, indices.graphicsFamily, 0, &_graphicsQueue);
            vkGetDeviceQueue(_device, indices.presentFamily, 0, &_presentQueue);
        }

        void ArxDevice::createCommandPool() {
            QueueFamilyIndices queueFamilyIndices = findPhysicalQueueFamilies();

            VkCommandPoolCreateInfo poolInfo = {};
            poolInfo.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily;
            poolInfo.flags            = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT | VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            if (vkCreateCommandPool(_device, &poolInfo, nullptr, &commandPool) != VK_SUCCESS) {
                throw std::runtime_error("failed to create command pool!");
                }
        }

        void ArxDevice::createSurface() { window.createWindowSurface(instance, &_surface); }

        bool ArxDevice::isDeviceSuitable(VkPhysicalDevice device) {
            QueueFamilyIndices indices = findQueueFamilies(device);

            bool extensionsSupported = checkDeviceExtensionSupport(device);

            bool swapChainAdequate = false;
            if (extensionsSupported) {
                SwapChainSupportDetails swapChainSupport = querySwapChainSupport(device);
                swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            }

            VkPhysicalDeviceFeatures supportedFeatures;
            vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

            return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
        }

        void ArxDevice::setImagelessFramebufferFeature()
        {
            imagelessFramebufferFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGELESS_FRAMEBUFFER_FEATURES;
            imagelessFramebufferFeatures.pNext = nullptr;
            
            // Query the supported features
            VkPhysicalDeviceFeatures2 deviceFeatures2 = {};
            deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            deviceFeatures2.pNext = &imagelessFramebufferFeatures;
            vkGetPhysicalDeviceFeatures2(physicalDevice, &deviceFeatures2);
            
            // Check if the feature is supported and enabled
            if (imagelessFramebufferFeatures.imagelessFramebuffer == VK_TRUE) {
                // Set the feature to VK_TRUE
                imagelessFramebufferFeatures.imagelessFramebuffer = VK_TRUE;
            }
            else
                throw std::runtime_error("failed to set imagelessFramebuffer!");
        }

        void ArxDevice::populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
            createInfo = {};
            createInfo.sType              = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity    = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType        = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback    = debugCallback;
            createInfo.pUserData          = nullptr;  // Optional
        }

        void ArxDevice::setupDebugMessenger() {
            if (!enableValidationLayers) return;
            VkDebugUtilsMessengerCreateInfoEXT createInfo;
            populateDebugMessengerCreateInfo(createInfo);
            if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
                throw std::runtime_error("failed to set up debug messenger!");
            }
        }

        bool ArxDevice::checkValidationLayerSupport() {
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

            for (const char *layerName : validationLayers) {
                bool layerFound = false;
                for (const auto &layerProperties : availableLayers) {
                  if (strcmp(layerName, layerProperties.layerName) == 0) {
                    layerFound = true;
                    break;
                  }
                }

                if (!layerFound) {
                  return false;
                }
            }

            return true;
        }

        std::vector<const char *> ArxDevice::getRequiredExtensions() {
            uint32_t glfwExtensionCount = 0;
            const char **glfwExtensions;
            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            std::vector<const char *> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            if (enableValidationLayers) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
            }

            return extensions;
        }

        void ArxDevice::hasGflwRequiredInstanceExtensions() {
            uint32_t extensionCount = 0;
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);
            std::vector<VkExtensionProperties> extensions(extensionCount);
            vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, extensions.data());

//            std::cout << "available extensions:" << std::endl;
            std::unordered_set<std::string> available;
            for (const auto &extension : extensions) {
//                std::cout << "\t" << extension.extensionName << std::endl;
                available.insert(extension.extensionName);
            }

            //      std::cout << "required extensions:" << std::endl;
            auto requiredExtensions = getRequiredExtensions();
            for (const auto &required : requiredExtensions) {
                //        std::cout << "\t" << required << std::endl;
                if (available.find(required) == available.end()) {
                  throw std::runtime_error("Missing required glfw extension");
                }
            }
        }

        bool ArxDevice::checkDeviceExtensionSupport(VkPhysicalDevice device) {
            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(
              device,
              nullptr,
              &extensionCount,
              availableExtensions.data());

            std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());

            for (const auto &extension : availableExtensions) {
                requiredExtensions.erase(extension.extensionName);
            }

            return requiredExtensions.empty();
        }

        QueueFamilyIndices ArxDevice::findQueueFamilies(VkPhysicalDevice device) {
            QueueFamilyIndices indices;

            uint32_t queueFamilyCount = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

            std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

            int i = 0;
            for (const auto &queueFamily : queueFamilies) {
                if (queueFamily.queueCount > 0 && queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
                  indices.graphicsFamily = i;
                  indices.graphicsFamilyHasValue = true;
                }
                VkBool32 presentSupport = false;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, _surface, &presentSupport);
                if (queueFamily.queueCount > 0 && presentSupport) {
                  indices.presentFamily = i;
                  indices.presentFamilyHasValue = true;
                }
                if (indices.isComplete()) {
                  break;
                }
                
                i++;
            }

                return indices;
            }

            SwapChainSupportDetails ArxDevice::querySwapChainSupport(VkPhysicalDevice device) {
            SwapChainSupportDetails details;
            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, _surface, &details.capabilities);

            uint32_t formatCount;
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, nullptr);

            if (formatCount != 0) {
                details.formats.resize(formatCount);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, _surface, &formatCount, details.formats.data());
            }

            uint32_t presentModeCount;
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, _surface, &presentModeCount, nullptr);

            if (presentModeCount != 0) {
            details.presentModes.resize(presentModeCount);
            vkGetPhysicalDeviceSurfacePresentModesKHR(
                device,
                _surface,
                &presentModeCount,
                details.presentModes.data());
            }
            return details;
        }

        VkSampleCountFlagBits ArxDevice::getMaxUsableSampleCount() {
            VkPhysicalDeviceProperties physicalDeviceProperties;
            vkGetPhysicalDeviceProperties(physicalDevice, &physicalDeviceProperties);
            
            VkSampleCountFlags counts = physicalDeviceProperties.limits.framebufferColorSampleCounts & physicalDeviceProperties.limits.framebufferDepthSampleCounts;
            if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
            if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
            if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
            if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
            if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
            if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }
            
            return VK_SAMPLE_COUNT_1_BIT;
        }

        VkFormat ArxDevice::findSupportedFormat(const std::vector<VkFormat> &candidates, VkImageTiling tiling, VkFormatFeatureFlags features) {
            for (VkFormat format : candidates) {
                VkFormatProperties props;
                vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);

                if (tiling == VK_IMAGE_TILING_LINEAR && (props.linearTilingFeatures & features) == features) {
                  return format;
                } else if (
                    tiling == VK_IMAGE_TILING_OPTIMAL && (props.optimalTilingFeatures & features) == features) {
                      return format;
                    }
                }
                throw std::runtime_error("failed to find supported format!");
            }

            uint32_t ArxDevice::findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
            VkPhysicalDeviceMemoryProperties memProperties;
            vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);
            for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
                if ((typeFilter & (1 << i)) &&
                    (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
                  return i;
                }
            }

            throw std::runtime_error("failed to find suitable memory type!");
        }

        void ArxDevice::createBuffer(
        VkDeviceSize size,
        VkBufferUsageFlags usage,
        VkMemoryPropertyFlags properties,
        VkBuffer &buffer,
        VkDeviceMemory &bufferMemory) {
            VkBufferCreateInfo bufferInfo{};
            bufferInfo.sType          = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.size           = size;
            bufferInfo.usage          = usage;
            bufferInfo.sharingMode    = VK_SHARING_MODE_EXCLUSIVE;

            if (vkCreateBuffer(_device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
                throw std::runtime_error("failed to create vertex buffer!");
            }

            VkMemoryRequirements memRequirements;
            vkGetBufferMemoryRequirements(_device, buffer, &memRequirements);

            VkMemoryAllocateInfo allocInfo{};
            allocInfo.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            allocInfo.allocationSize  = memRequirements.size;
            allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

            if (vkAllocateMemory(_device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
                throw std::runtime_error("failed to allocate vertex buffer memory!");
            }

            vkBindBufferMemory(_device, buffer, bufferMemory, 0);
        }

        VkCommandBuffer ArxDevice::beginSingleTimeCommands() {
            VkCommandBufferAllocateInfo allocInfo{};
            allocInfo.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            allocInfo.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            allocInfo.commandPool        = commandPool;
            allocInfo.commandBufferCount = 1;

            VkCommandBuffer commandBuffer;
            vkAllocateCommandBuffers(_device, &allocInfo, &commandBuffer);

            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            vkBeginCommandBuffer(commandBuffer, &beginInfo);
            return commandBuffer;
        }

        void ArxDevice::endSingleTimeCommands(VkCommandBuffer commandBuffer) {
            vkEndCommandBuffer(commandBuffer);

            VkSubmitInfo submitInfo{};
            submitInfo.sType                  = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            submitInfo.commandBufferCount     = 1;
            submitInfo.pCommandBuffers        = &commandBuffer;

            vkQueueSubmit(_graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
            vkQueueWaitIdle(_graphicsQueue);

            vkFreeCommandBuffers(_device, commandPool, 1, &commandBuffer);
        }

        void ArxDevice::copyBuffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
            VkCommandBuffer commandBuffer = beginSingleTimeCommands();

            VkBufferCopy copyRegion{};
            copyRegion.srcOffset = 0;  // Optional
            copyRegion.dstOffset = 0;  // Optional
            copyRegion.size = size;
            vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

            endSingleTimeCommands(commandBuffer);
        }

        void ArxDevice::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height, uint32_t layerCount) {
        VkCommandBuffer commandBuffer = beginSingleTimeCommands();

        VkBufferImageCopy region{};
        region.bufferOffset       = 0;
        region.bufferRowLength    = 0;
        region.bufferImageHeight  = 0;

        region.imageSubresource.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel          = 0;
        region.imageSubresource.baseArrayLayer    = 0;
        region.imageSubresource.layerCount        = layerCount;

        region.imageOffset                        = {0, 0, 0};
        region.imageExtent = {width, height, 1};

        vkCmdCopyBufferToImage(
              commandBuffer,
              buffer,
              image,
              VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
              1,
              &region);
            endSingleTimeCommands(commandBuffer);
        }

        void ArxDevice::destroyBuffer(VkBuffer buffer) {
            vkDestroyBuffer(device(), buffer, nullptr);
        }

        void ArxDevice::freeMemory(VkDeviceMemory memory) {
            vkFreeMemory(device(), memory, nullptr);
        }


        void ArxDevice::createImageWithInfo(
            const VkImageCreateInfo &imageInfo,
            VkMemoryPropertyFlags properties,
            VkImage &image,
            VkDeviceMemory &imageMemory) {
        if (vkCreateImage(_device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(_device, image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(_device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        if (vkBindImageMemory(_device, image, imageMemory, 0) != VK_SUCCESS) {
            throw std::runtime_error("failed to bind image memory!");
        }
    }
}
