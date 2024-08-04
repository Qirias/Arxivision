#include "arx_depthPyramid_manager.hpp"


namespace arx {

    ArxDepthPyramidManager::ArxDepthPyramidManager(ArxDevice& device, VkExtent2D extent)
    : device(device), depthPyramidMips(16) {
        createDepthResources(extent);
        createDepthSampler();
        // Other initialization as needed
    }

    void ArxDepthPyramidManager::createDepthResources(VkExtent2D extent) {
        
    }

    void ArxDepthPyramidManager::updateDepthPyramid(VkCommandBuffer cmdBuffer) {
        
        for (int32_t i = 0; i < depthPyramidMips.size(); ++i) {
            VkDescriptorImageInfo destTarget;
            destTarget.sampler = depthSampler;
            destTarget.imageView = depthPyramidMips[i];
            destTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

            VkDescriptorImageInfo sourceTarget;
            sourceTarget.sampler = depthSampler;
            if (i == 0) {
                // For the first iteration, we grab it from the depth image
                sourceTarget.imageView = getDepthImageView(); // Assuming depthImageView is the view for the depthImage
                sourceTarget.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            } else {
                // Afterwards, we copy from a depth mipmap into the next
                sourceTarget.imageView = depthPyramidMips[i - 1];
                sourceTarget.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            }

            // Assume vkutil::DescriptorBuilder is a utility you have for building descriptors
            VkDescriptorSet depthSet = /* Initialize and build descriptor set with destTarget and sourceTarget */;

            // Bind the descriptor set for the compute shader
            vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, /* depthReducePipelineLayout */, 0, 1, &depthSet, 0, nullptr);

            uint32_t levelWidth = /* width of the current mip level */;
            uint32_t levelHeight = /* height of the current mip level */;

            // Assume DepthReduceData is a struct that matches the push constant in your shader
            DepthReduceData reduceData = { glm::vec2(levelWidth, levelHeight) };

            // Execute downsample compute shader
            vkCmdPushConstants(cmdBuffer, /* depthReducePipelineLayout */, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(reduceData), &reduceData);
            vkCmdDispatch(cmdBuffer, /* compute dispatch parameters based on levelWidth and levelHeight */);

            // Pipeline barrier before doing the next mipmap
            VkImageMemoryBarrier reduceBarrier = /* Initialize barrier for transitioning the mip level */;
            vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &reduceBarrier);
        }
    }
}
