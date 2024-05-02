#pragma once

#include "arx_device.h"

// std
#include <memory>
#include <unordered_map>
#include <vector>

namespace arx {

    class ArxDescriptorSetLayout {
     public:
      class Builder {
       public:
        Builder(ArxDevice &arxDevice) : arxDevice{arxDevice} {}

        Builder &addBinding(
            uint32_t binding,
            VkDescriptorType descriptorType,
            VkShaderStageFlags stageFlags,
            uint32_t count = 1);
        std::unique_ptr<ArxDescriptorSetLayout> build() const;

       private:
        ArxDevice &arxDevice;
        std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings{};
      };

      ArxDescriptorSetLayout(ArxDevice &arxDevice, std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings);
      ~ArxDescriptorSetLayout();
      ArxDescriptorSetLayout(const ArxDescriptorSetLayout &) = delete;
      ArxDescriptorSetLayout &operator=(const ArxDescriptorSetLayout &) = delete;

      VkDescriptorSetLayout getDescriptorSetLayout() const { return descriptorSetLayout; }

     private:
      ArxDevice &arxDevice;
      VkDescriptorSetLayout descriptorSetLayout;
      std::unordered_map<uint32_t, VkDescriptorSetLayoutBinding> bindings;

      friend class ArxDescriptorWriter;
    };

    class ArxDescriptorPool {
    public:
    class Builder {
    public:
        Builder(ArxDevice &arxDevice) : arxDevice{arxDevice} {}

        Builder &addPoolSize(VkDescriptorType descriptorType, uint32_t count);
        Builder &setPoolFlags(VkDescriptorPoolCreateFlags flags);
        Builder &setMaxSets(uint32_t count);
        std::unique_ptr<ArxDescriptorPool> build() const;

        private:
        ArxDevice &arxDevice;
        std::vector<VkDescriptorPoolSize> poolSizes{};
        uint32_t maxSets = 1000;
        VkDescriptorPoolCreateFlags poolFlags = 0;
    };

        ArxDescriptorPool(
            ArxDevice &arxDevice,
            uint32_t maxSets,
            VkDescriptorPoolCreateFlags poolFlags,
        const std::vector<VkDescriptorPoolSize> &poolSizes);
        ~ArxDescriptorPool();
        ArxDescriptorPool(const ArxDescriptorPool &) = delete;
        ArxDescriptorPool &operator=(const ArxDescriptorPool &) = delete;

        bool allocateDescriptor(
        const VkDescriptorSetLayout descriptorSetLayout, VkDescriptorSet &descriptor) const;

        void freeDescriptors(std::vector<VkDescriptorSet> &descriptors) const;

        void resetPool();

        VkDescriptorPool getDescriptorPool() const {return descriptorPool; }
    private:
        ArxDevice &arxDevice;
        VkDescriptorPool descriptorPool;

        friend class ArxDescriptorWriter;
    };

    class ArxDescriptorWriter {
     public:
      ArxDescriptorWriter(ArxDescriptorSetLayout &setLayout, ArxDescriptorPool &pool);

      ArxDescriptorWriter &writeBuffer(uint32_t binding, VkDescriptorBufferInfo *bufferInfo);
      ArxDescriptorWriter &writeImage(uint32_t binding, VkDescriptorImageInfo *imageInfo);

      bool build(VkDescriptorSet &set);
      void overwrite(VkDescriptorSet &set);

     private:
      ArxDescriptorSetLayout &setLayout;
      ArxDescriptorPool &pool;
      std::vector<VkWriteDescriptorSet> writes;
    };
}
