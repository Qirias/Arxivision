#pragma once
#include "arx_device.h"
#include "arx_descriptors.h"

class ArxSSAOResources {
public:
    ArxSSAOResources(ArxDevice& device);

    void createDescriptorSetLayouts();
    void createDescriptorPool();
    void allocateDescriptorSets();

    // Accessors for descriptor sets and layouts
    VkDescriptorSet getGBufferDescriptorSet() const;
    VkDescriptorSetLayout getGBufferDescriptorSetLayout() const;
    // ... add accessors for ssao, ssaoBlur, composition as needed

private:
    ArxDevice&                              device;
    std::unique_ptr<ArxDescriptorSetLayout> gBufferLayout;
    VkDescriptorSet gBufferDescriptorSet;
    // Plus others for ssao, ssaoBlur, composition
    std::unique_ptr<ArxDescriptorPool>      descriptorPool;

    // Internal methods to setup descriptor sets and layouts
};

