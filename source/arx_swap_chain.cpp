#include "arx_swap_chain.h"

// std
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <set>
#include <stdexcept>
#include <fstream>

namespace arx {

    ArxSwapChain::ArxSwapChain(ArxDevice &deviceRef, VkExtent2D extent)
        : device{deviceRef}, windowExtent{extent}, cull(deviceRef) {
        init();
    }

    ArxSwapChain::ArxSwapChain(ArxDevice &deviceRef, VkExtent2D extent, std::shared_ptr<ArxSwapChain> previous)
        : device{deviceRef}, windowExtent{extent}, oldSwapChain(previous), cull(deviceRef) {
        init();
            
        // clean up old swap chain since it's no longer needed
        oldSwapChain = nullptr;
    }

    void ArxSwapChain::init() {
        createSwapChain();
        createImageViews();
        createRenderPass();
//        createRenderPass(true); // late
        createColorResources();
        createDepthResources();
        createFramebuffers();
//        createFramebuffers(true); // late
        createSyncObjects();
        
        createDepthSampler();
        createDepthPyramid();
        createDepthPyramidDescriptors();
        createBarriers();
    }

    ArxSwapChain::~ArxSwapChain() {
        vkDestroyImageView(device.device(), colorImageView, nullptr);
        vkDestroyImage(device.device(), colorImage, nullptr);
        vkFreeMemory(device.device(), colorImageMemory, nullptr);

        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device.device(), imageView, nullptr);
        }
        swapChainImageViews.clear();

        if (swapChain != nullptr) {
            vkDestroySwapchainKHR(device.device(), swapChain, nullptr);
            swapChain = nullptr;
        }

        for (int i = 0; i < depthImages.size(); i++) {
            vkDestroyImageView(device.device(), depthImageViews[i], nullptr);
            vkDestroyImage(device.device(), depthImages[i], nullptr);
            vkFreeMemory(device.device(), depthImageMemorys[i], nullptr);
        }
        vkDestroyImage(device.device(), depthImage, nullptr);
        vkDestroyImageView(device.device(), depthImageView, nullptr);
        vkFreeMemory(device.device(), depthImageMemory, nullptr);
        
        vkDestroySampler(device.device(), depthSampler, nullptr);
        depthSampler = VK_NULL_HANDLE;

        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device.device(), framebuffer, nullptr);
        }

        vkDestroyRenderPass(device.device(), renderPass, nullptr);
        vkDestroyRenderPass(device.device(), lateRenderPass, nullptr);

        // cleanup synchronization objects
        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
            vkDestroySemaphore(device.device(), renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(device.device(), imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(device.device(), inFlightFences[i], nullptr);
        }
        
        // depth pyramid
        for (VkImageView view : cull.depthPyramidMips) {
            vkDestroyImageView(device.device(), view, nullptr);
        }
        cull.depthPyramidMips.clear();
        
        vkDestroyImage(device.device(), cull.depthPyramidImage, nullptr);
        vkFreeMemory(device.device(), cull.depthPyramidMemory, nullptr);
        
        vkDestroyPipeline(device.device(), cull.depthPyramidPipeline->computePipeline, nullptr);
        cull.depthPyramidPipeline->computePipeline = VK_NULL_HANDLE;
        
        vkDestroyPipelineLayout(device.device(), cull.depthPyramidPipelineLayout, nullptr);
        cull.depthPyramidPipelineLayout = VK_NULL_HANDLE;
        
        cull.depthDescriptorSets.clear();
        
        // culling
        cull.cullingDescriptorSet = VK_NULL_HANDLE;
        cull.cullingDescriptorLayout = VK_NULL_HANDLE;
    }

    

    VkResult ArxSwapChain::acquireNextImage(uint32_t *imageIndex) {
      vkWaitForFences(
          device.device(),
          1,
          &inFlightFences[currentFrame],
          VK_TRUE,
          std::numeric_limits<uint64_t>::max());

      VkResult result = vkAcquireNextImageKHR(
          device.device(),
          swapChain,
          std::numeric_limits<uint64_t>::max(),
          imageAvailableSemaphores[currentFrame],  // must be a not signaled semaphore
          VK_NULL_HANDLE,
          imageIndex);

      return result;
    }

    VkResult ArxSwapChain::submitCommandBuffers(const VkCommandBuffer *buffers, uint32_t *imageIndex) {
      if (imagesInFlight[*imageIndex] != VK_NULL_HANDLE) {
        vkWaitForFences(device.device(), 1, &imagesInFlight[*imageIndex], VK_TRUE, UINT64_MAX);
      }
      imagesInFlight[*imageIndex] = inFlightFences[currentFrame];

      VkSubmitInfo submitInfo = {};
      submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

      VkSemaphore waitSemaphores[]      = {imageAvailableSemaphores[currentFrame]};
      VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
      submitInfo.waitSemaphoreCount     = 1;
      submitInfo.pWaitSemaphores        = waitSemaphores;
      submitInfo.pWaitDstStageMask      = waitStages;

      submitInfo.commandBufferCount     = 1;
      submitInfo.pCommandBuffers        = buffers;

      VkSemaphore signalSemaphores[]    = {renderFinishedSemaphores[currentFrame]};
      submitInfo.signalSemaphoreCount   = 1;
      submitInfo.pSignalSemaphores      = signalSemaphores;

      vkResetFences(device.device(), 1, &inFlightFences[currentFrame]);
      if (vkQueueSubmit(device.graphicsQueue(), 1, &submitInfo, inFlightFences[currentFrame]) !=
          VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
      }

      VkPresentInfoKHR presentInfo = {};
      presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;

      presentInfo.waitSemaphoreCount    = 1;
      presentInfo.pWaitSemaphores       = signalSemaphores;

      VkSwapchainKHR swapChains[]   = {swapChain};
      presentInfo.swapchainCount    = 1;
      presentInfo.pSwapchains       = swapChains;

      presentInfo.pImageIndices     = imageIndex;

      auto result = vkQueuePresentKHR(device.presentQueue(), &presentInfo);

      currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

      return result;
    }

    void ArxSwapChain::createSwapChain() {
      SwapChainSupportDetails swapChainSupport = device.getSwapChainSupport();

      VkSurfaceFormatKHR surfaceFormat  = chooseSwapSurfaceFormat(swapChainSupport.formats);
      VkPresentModeKHR presentMode      = chooseSwapPresentMode(swapChainSupport.presentModes);
      VkExtent2D extent                 = chooseSwapExtent(swapChainSupport.capabilities);

      uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
      if (swapChainSupport.capabilities.maxImageCount > 0 &&
          imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
      }

      VkSwapchainCreateInfoKHR createInfo = {};
      createInfo.sType              = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
      createInfo.surface            = device.surface();

      createInfo.minImageCount      = imageCount;
      createInfo.imageFormat        = surfaceFormat.format;
      createInfo.imageColorSpace    = surfaceFormat.colorSpace;
      createInfo.imageExtent        = extent;
      createInfo.imageArrayLayers   = 1;
      createInfo.imageUsage         = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

      QueueFamilyIndices indices    = device.findPhysicalQueueFamilies();
      uint32_t queueFamilyIndices[] = {indices.graphicsFamily, indices.presentFamily};

      if (indices.graphicsFamily != indices.presentFamily) {
        createInfo.imageSharingMode         = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount    = 2;
        createInfo.pQueueFamilyIndices      = queueFamilyIndices;
      } else {
        createInfo.imageSharingMode         = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.queueFamilyIndexCount    = 0; // Optional
        createInfo.pQueueFamilyIndices      = nullptr;  // Optional
      }

      createInfo.preTransform   = swapChainSupport.capabilities.currentTransform;
      createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

      createInfo.presentMode    = presentMode;
      createInfo.clipped        = VK_TRUE;

      createInfo.oldSwapchain   = oldSwapChain == nullptr ? VK_NULL_HANDLE : oldSwapChain->swapChain;

      if (vkCreateSwapchainKHR(device.device(), &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
        throw std::runtime_error("failed to create swap chain!");
      }

      // we only specified a minimum number of images in the swap chain, so the implementation is
      // allowed to create a swap chain with more. That's why we'll first query the final number of
      // images with vkGetSwapchainImagesKHR, then resize the container and finally call it again to
      // retrieve the handles.
      vkGetSwapchainImagesKHR(device.device(), swapChain, &imageCount, nullptr);
      swapChainImages.resize(imageCount);
      vkGetSwapchainImagesKHR(device.device(), swapChain, &imageCount, swapChainImages.data());

      swapChainImageFormat  = surfaceFormat.format;
      swapChainExtent       = extent;
    }

    void ArxSwapChain::createImageViews() {
      swapChainImageViews.resize(swapChainImages.size());
      for (size_t i = 0; i < swapChainImages.size(); i++) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                              = swapChainImages[i];
        viewInfo.viewType                           = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                             = swapChainImageFormat;
        viewInfo.subresourceRange.aspectMask        = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel      = 0;
        viewInfo.subresourceRange.levelCount        = 1;
        viewInfo.subresourceRange.baseArrayLayer    = 0;
        viewInfo.subresourceRange.layerCount        = 1;

        if (vkCreateImageView(device.device(), &viewInfo, nullptr, &swapChainImageViews[i]) !=
            VK_SUCCESS) {
          throw std::runtime_error("failed to create texture image view!");
        }
      }
    }

    void ArxSwapChain::createRenderPass(bool latePass) {
        VkAttachmentDescription2 depthAttachment{};
        depthAttachment.sType             = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
        depthAttachment.format            = findDepthFormat();
        depthAttachment.samples           = device.msaaSamples;
        depthAttachment.loadOp            = latePass ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp           = latePass ? VK_ATTACHMENT_STORE_OP_DONT_CARE: VK_ATTACHMENT_STORE_OP_STORE;
        depthAttachment.stencilLoadOp     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp    = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout     = latePass ? VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout       = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        VkAttachmentReference2 depthAttachmentRef{};
        depthAttachmentRef.sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        
        // New single-sampled depth attachment for depth resolve
        VkAttachmentDescription2 depthResolveAttachment = {};
        depthResolveAttachment.sType  = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
        depthResolveAttachment.format = findDepthFormat();
        depthResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        depthResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthResolveAttachment.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        
        VkAttachmentReference2 depthResolveAttachmentRef = {};
        depthResolveAttachmentRef.sType = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
        depthResolveAttachmentRef.attachment = 3; // index of single-sampled resolve depth attachment
        depthResolveAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        depthResolveAttachmentRef.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

        VkAttachmentDescription2 colorAttachment = {};
        colorAttachment.sType             = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
        colorAttachment.format            = getSwapChainImageFormat();
        colorAttachment.samples           = device.msaaSamples;
        colorAttachment.loadOp            = latePass ? VK_ATTACHMENT_LOAD_OP_LOAD : VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp           = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilStoreOp    = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.stencilLoadOp     = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.initialLayout     = latePass ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL : VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout       = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference2 colorAttachmentRef = {};
        colorAttachmentRef.sType      = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentDescription2 colorAttachmentResolve{};
        colorAttachmentResolve.sType            = VK_STRUCTURE_TYPE_ATTACHMENT_DESCRIPTION_2;
        colorAttachmentResolve.format           = getSwapChainImageFormat();
        colorAttachmentResolve.samples          = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentResolve.loadOp           = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.storeOp          = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachmentResolve.stencilLoadOp    = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentResolve.stencilStoreOp   = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentResolve.initialLayout    = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentResolve.finalLayout      = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference2 colorAttachmentResolveRef{};
        colorAttachmentResolveRef.sType         = VK_STRUCTURE_TYPE_ATTACHMENT_REFERENCE_2;
        colorAttachmentResolveRef.attachment    = 2;
        colorAttachmentResolveRef.layout        = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        
        VkSubpassDescriptionDepthStencilResolve subpassDepthSencilResolve = {};
        subpassDepthSencilResolve.sType                             = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_DEPTH_STENCIL_RESOLVE;
        subpassDepthSencilResolve.stencilResolveMode                = VK_RESOLVE_MODE_NONE;
        subpassDepthSencilResolve.depthResolveMode                  = VK_RESOLVE_MODE_SAMPLE_ZERO_BIT;
        subpassDepthSencilResolve.pDepthStencilResolveAttachment    = &depthResolveAttachmentRef;

        VkSubpassDescription2 subpass = {};
        subpass.sType                     = VK_STRUCTURE_TYPE_SUBPASS_DESCRIPTION_2;
        subpass.pipelineBindPoint         = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount      = 1;
        subpass.pColorAttachments         = &colorAttachmentRef;
        subpass.pDepthStencilAttachment   = &depthAttachmentRef;
        subpass.pResolveAttachments       = &colorAttachmentResolveRef;
        subpass.pNext = &subpassDepthSencilResolve;
    
        VkSubpassDependency2 dependency = {};
        dependency.sType          = VK_STRUCTURE_TYPE_SUBPASS_DEPENDENCY_2;
        dependency.srcSubpass     = VK_SUBPASS_EXTERNAL;
        dependency.srcAccessMask  = 0;
        dependency.srcStageMask   = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstSubpass     = 0;
        dependency.dstStageMask   = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask  = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

        std::array<VkAttachmentDescription2, 4> attachments = {colorAttachment, depthAttachment, colorAttachmentResolve, depthResolveAttachment};
        VkRenderPassCreateInfo2 renderPassInfo = {};
        renderPassInfo.sType              = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO_2;
        renderPassInfo.attachmentCount    = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments       = attachments.data();
        renderPassInfo.subpassCount       = 1;
        renderPassInfo.pSubpasses         = &subpass;
        renderPassInfo.dependencyCount    = 1;
        renderPassInfo.pDependencies      = &dependency;

        if (vkCreateRenderPass2(device.device(), &renderPassInfo, nullptr, latePass ? &lateRenderPass : &renderPass) != VK_SUCCESS) {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void ArxSwapChain::createFramebuffers(bool latePass) {
        if (latePass)
            lateSwapChainFramebuffers.resize(imageCount());
        else
            swapChainFramebuffers.resize(imageCount());

        for (size_t i = 0; i < imageCount(); i++) {
            std::array<VkImageView, 4> attachments = {colorImageView, depthImageViews[i], swapChainImageViews[i], depthImageView};

            VkExtent2D swapChainExtent = getSwapChainExtent();
            VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass      = renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments    = attachments.data();
            framebufferInfo.width           = swapChainExtent.width;
            framebufferInfo.height          = swapChainExtent.height;
            framebufferInfo.layers          = 1;

            if (vkCreateFramebuffer(
                    device.device(),
                    &framebufferInfo,
                    nullptr,
                    latePass ? &lateSwapChainFramebuffers[i] : &swapChainFramebuffers[i]) != VK_SUCCESS) {
              throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void ArxSwapChain::createColorResources() {
        VkFormat colorFormat = getSwapChainImageFormat();
        
        createImage(swapChainExtent.width, swapChainExtent.height, 1, device.msaaSamples, colorFormat, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, colorImage, colorImageMemory);
        
        colorImageView = createImageView(colorImage, colorFormat, VK_IMAGE_ASPECT_COLOR_BIT, 1);
    }

    VkImageView ArxSwapChain::createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, uint32_t mipLevels, uint32_t baseMipLevel) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType                              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image                              = image;
        viewInfo.viewType                           = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format                             = format;
        viewInfo.subresourceRange.aspectMask        = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel      = baseMipLevel;
        viewInfo.subresourceRange.levelCount        = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer    = 0;
        viewInfo.subresourceRange.layerCount        = 1;
        
        VkImageView imageView;
        if (vkCreateImageView(device.device(), &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image view!");
        }
        
        return imageView;
    }

    void ArxSwapChain::createImage(uint32_t width, uint32_t height, uint32_t mipLevels, VkSampleCountFlagBits numSamples, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImage& image, VkDeviceMemory& imageMemory) {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType     = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width  = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth  = 1;
        imageInfo.mipLevels     = 1;
        imageInfo.arrayLayers   = 1;
        imageInfo.format        = format;
        imageInfo.tiling        = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        imageInfo.usage         = usage;
        imageInfo.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.mipLevels     = mipLevels;
        imageInfo.samples       = numSamples;

        if (vkCreateImage(device.device(), &imageInfo, nullptr, &image) != VK_SUCCESS) {
            throw std::runtime_error("failed to create image!");
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device.device(), image, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = device.findMemoryType(memRequirements.memoryTypeBits, properties);

        if (vkAllocateMemory(device.device(), &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
            throw std::runtime_error("failed to allocate image memory!");
        }

        vkBindImageMemory(device.device(), image, imageMemory, 0);
    }

    void ArxSwapChain::createDepthResources() {
        VkFormat depthFormat          = findDepthFormat();
        swapChainDepthFormat          = depthFormat;
        VkExtent2D swapChainExtent    = getSwapChainExtent();

        depthImages.resize(imageCount());
        depthImageMemorys.resize(imageCount());
        depthImageViews.resize(imageCount());

        for (int i = 0; i < depthImages.size(); i++) {
            VkImageCreateInfo imageInfo{};
            imageInfo.sType             = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType         = VK_IMAGE_TYPE_2D;
            imageInfo.extent.width      = swapChainExtent.width;
            imageInfo.extent.height     = swapChainExtent.height;
            imageInfo.extent.depth      = 1;
            imageInfo.mipLevels         = 1;
            imageInfo.arrayLayers       = 1;
            imageInfo.format            = depthFormat;
            imageInfo.tiling            = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.initialLayout     = VK_IMAGE_LAYOUT_UNDEFINED;
            imageInfo.usage             = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
            imageInfo.samples           = device.msaaSamples;
            imageInfo.sharingMode       = VK_SHARING_MODE_EXCLUSIVE;
            imageInfo.flags             = 0;

            device.createImageWithInfo(
                imageInfo,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                depthImages[i],
                depthImageMemorys[i]);

            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType                              = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image                              = depthImages[i];
            viewInfo.viewType                           = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format                             = depthFormat;
            viewInfo.subresourceRange.aspectMask        = VK_IMAGE_ASPECT_DEPTH_BIT;
            viewInfo.subresourceRange.baseMipLevel      = 0;
            viewInfo.subresourceRange.levelCount        = 1;
            viewInfo.subresourceRange.baseArrayLayer    = 0;
            viewInfo.subresourceRange.layerCount        = 1;

            if (vkCreateImageView(device.device(), &viewInfo, nullptr, &depthImageViews[i]) != VK_SUCCESS) {
              throw std::runtime_error("failed to create texture image view!");
            }
        }
        
        // Create sinlge-sample depth image
        createImage(swapChainExtent.width, swapChainExtent.height, 1,
                    VK_SAMPLE_COUNT_1_BIT,
                    depthFormat,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    depthImage,
                    depthImageMemory);
        depthImageView = createImageView(depthImage, depthFormat, VK_IMAGE_ASPECT_DEPTH_BIT, 1);
    }

    void ArxSwapChain::createSyncObjects() {
      imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
      renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
      inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
      imagesInFlight.resize(imageCount(), VK_NULL_HANDLE);

      VkSemaphoreCreateInfo semaphoreInfo = {};
      semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

      VkFenceCreateInfo fenceInfo = {};
      fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
      fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

      for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &imageAvailableSemaphores[i]) !=
                VK_SUCCESS ||
            vkCreateSemaphore(device.device(), &semaphoreInfo, nullptr, &renderFinishedSemaphores[i]) !=
                VK_SUCCESS ||
            vkCreateFence(device.device(), &fenceInfo, nullptr, &inFlightFences[i]) != VK_SUCCESS) {
          throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
      }
    }

    VkSurfaceFormatKHR ArxSwapChain::chooseSwapSurfaceFormat(
        const std::vector<VkSurfaceFormatKHR> &availableFormats) {
      for (const auto &availableFormat : availableFormats) {
        if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
          return availableFormat;
        }
      }

      return availableFormats[0];
    }

    VkPresentModeKHR ArxSwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR> &availablePresentModes) {
      for (const auto &availablePresentMode : availablePresentModes) {
        if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
          std::cout << "Present mode: Mailbox" << std::endl;
          return availablePresentMode;
        }
      }

      // for (const auto &availablePresentMode : availablePresentModes) {
      //   if (availablePresentMode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
      //     std::cout << "Present mode: Immediate" << std::endl;
      //     return availablePresentMode;
      //   }
      // }

      std::cout << "Present mode: V-Sync" << std::endl;
      return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D ArxSwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR &capabilities) {
      if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
        return capabilities.currentExtent;
      } else {
        VkExtent2D actualExtent = windowExtent;
        actualExtent.width = std::max(
            capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, actualExtent.width));
        actualExtent.height = std::max(
            capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, actualExtent.height));

        return actualExtent;
      }
    }

    VkFormat ArxSwapChain::findDepthFormat() {
      return device.findSupportedFormat(
          {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT},
          VK_IMAGE_TILING_OPTIMAL,
          VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    }

    void ArxSwapChain::createDepthPyramid() {
        cull.depthPyramidWidth = cull.previousPow2(swapChainExtent.width);
        cull.depthPyramidHeight = cull.previousPow2(swapChainExtent.height);
        
        uint32_t depthPyramidLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(swapChainExtent.width, swapChainExtent.height)))) + 1;
        cull.depthPyramidLevels = depthPyramidLevels;
        
        createImage(cull.depthPyramidWidth, cull.depthPyramidHeight, depthPyramidLevels,
                    VK_SAMPLE_COUNT_1_BIT,
                    VK_FORMAT_R32_SFLOAT,
                    VK_IMAGE_TILING_OPTIMAL,
                    VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                    cull.depthPyramidImage,
                    cull.depthPyramidMemory);
        
        cull.depthPyramidImageView = createImageView(cull.depthPyramidImage, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, cull.depthPyramidLevels);

        VkCommandBuffer cmd = device.beginSingleTimeCommands();
        VkImageMemoryBarrier depthPyramidLayoutBarrier = createImageBarrier(VK_IMAGE_LAYOUT_UNDEFINED,
                                                                           VK_IMAGE_LAYOUT_GENERAL,
                                                                           cull.depthPyramidImage,
                                                                           VK_IMAGE_ASPECT_COLOR_BIT,
                                                                           0, 0, 0,
                                                                           cull.depthPyramidLevels);
        
        vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &depthPyramidLayoutBarrier);
        device.endSingleTimeCommands(cmd);
        
        cull.depthPyramidMips.resize(cull.depthPyramidLevels, VK_NULL_HANDLE);
        for (uint32_t i = 0; i < cull.depthPyramidLevels; ++i) {
            cull.depthPyramidMips[i] = createImageView(cull.depthPyramidImage, VK_FORMAT_R32_SFLOAT, VK_IMAGE_ASPECT_COLOR_BIT, 1, i);
        }
    }

    void ArxSwapChain::createDepthSampler() {
        VkSamplerCreateInfo samplerCreateInfo = {};
        samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
        samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerCreateInfo.minLod = 0;
        samplerCreateInfo.maxLod = 16.f;
        
//        VkSamplerReductionModeCreateInfo reductionCreateInfo = {};
//        reductionCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_REDUCTION_MODE_CREATE_INFO;
//        reductionCreateInfo.reductionMode = VK_SAMPLER_REDUCTION_MODE_MAX;
//        samplerCreateInfo.pNext = &reductionCreateInfo;
//        
        if (vkCreateSampler(device.device(), &samplerCreateInfo, 0, &depthSampler) != VK_SUCCESS)
            throw std::runtime_error("Couldn't create depth sampler!");
    }

    VkImageMemoryBarrier ArxSwapChain::createImageBarrier(VkImageLayout oldLayout, VkImageLayout newLayout, VkImage image, VkImageAspectFlags aspectFlags, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, uint32_t baseMipLevels, uint32_t levelCount)
    {
        VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = oldLayout;
        barrier.newLayout = newLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = image;
        barrier.subresourceRange.aspectMask = aspectFlags;
        barrier.subresourceRange.baseMipLevel = baseMipLevels;
        barrier.subresourceRange.levelCount = levelCount;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;

        return barrier;
    }

    void ArxSwapChain::createCullingDescriptors() {
        cull.cullingDescriptorPool = ArxDescriptorPool::Builder(device)
            .setMaxSets(1)
            .addPoolSize(VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3.f)
            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1.f)
            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3.f)
            .build();
    
        VkDescriptorBufferInfo cameraBufferInfo{};
        cameraBufferInfo.buffer = cull.cameraBuffer->getBuffer();
        cameraBufferInfo.offset = 0;
        cameraBufferInfo.range = VK_WHOLE_SIZE;
        
        VkDescriptorBufferInfo objectsDataBufferInfo{};
        objectsDataBufferInfo.buffer = cull.objectsDataBuffer->getBuffer();
        objectsDataBufferInfo.offset = 0;
        objectsDataBufferInfo.range = VK_WHOLE_SIZE;
        
        VkDescriptorImageInfo depthPyramidInfo{};
        depthPyramidInfo.sampler = depthSampler;
        depthPyramidInfo.imageView = cull.depthPyramidImageView;
        depthPyramidInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        
        VkDescriptorBufferInfo visibilityInfo{};
        visibilityInfo.buffer = cull.visibilityBuffer->getBuffer();
        visibilityInfo.offset = 0;
        visibilityInfo.range = VK_WHOLE_SIZE;
        
        VkDescriptorBufferInfo globalDataBufferInfo{};
        globalDataBufferInfo.buffer = cull.globalDataBuffer->getBuffer();
        globalDataBufferInfo.offset = 0;
        globalDataBufferInfo.range = VK_WHOLE_SIZE;
        
        VkDescriptorBufferInfo miscBufferInfo{};
        miscBufferInfo.buffer = cull.miscBuffer->getBuffer();
        miscBufferInfo.offset = 0;
        miscBufferInfo.range = VK_WHOLE_SIZE;
        
        
        ArxDescriptorWriter(*cull.cullingDescriptorLayout, *cull.cullingDescriptorPool)
                            .writeBuffer(0, &cameraBufferInfo)
                            .writeBuffer(1, &objectsDataBufferInfo)
                            .writeImage(2, &depthPyramidInfo)
                            .writeBuffer(3, &visibilityInfo)
                            .writeBuffer(4, &globalDataBufferInfo)
                            .writeBuffer(5, &miscBufferInfo)
                            .build(cull.cullingDescriptorSet);
    }

    void ArxSwapChain::createDepthPyramidDescriptors() {
        cull.depthDescriptorSets.resize(cull.depthPyramidLevels, VK_NULL_HANDLE);
        
        cull.depthDescriptorPool = ArxDescriptorPool::Builder(device)
                            .setMaxSets(cull.depthPyramidLevels)
                            .addPoolSize(VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, cull.depthPyramidLevels)
                            .addPoolSize(VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, cull.depthPyramidLevels)
                            .build();

        
        for (uint32_t i = 0; i < cull.depthPyramidLevels; i++) {
            VkDescriptorImageInfo srcInfo = {};
            if (i == 0) {
                srcInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                srcInfo.imageView = depthImageView;
            }
            else {
                srcInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
                srcInfo.imageView = cull.depthPyramidMips[i-1];
            }
            srcInfo.sampler = depthSampler;

            VkDescriptorImageInfo dstInfo;
            dstInfo.sampler = depthSampler;
            dstInfo.imageView = cull.depthPyramidMips[i];
            dstInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            
            ArxDescriptorWriter(*cull.depthDescriptorLayout, *cull.depthDescriptorPool)
                .writeImage(0, &dstInfo)
                .writeImage(1, &srcInfo)
                .build(cull.depthDescriptorSets[i]);
        }
    }

    void ArxSwapChain::computeDepthPyramid(VkCommandBuffer commandBuffer) {
        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &cull.framebufferDepthWriteBarrier);
        
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cull.depthPyramidPipeline->computePipeline);
        
        for (uint32_t i = 0; i < cull.depthPyramidLevels; ++i)
        {
            vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, cull.depthPyramidPipelineLayout, 0, 1, &cull.depthDescriptorSets[i], 0, nullptr);

            uint32_t levelWidth = glm::max(1u, cull.depthPyramidWidth >> i);
            uint32_t levelHeight = glm::max(1u, cull.depthPyramidHeight >> i);

            glm::vec2 levelSize(levelWidth, levelHeight);

            uint32_t groupCountX = (levelWidth + 32 - 1) / 32;
            uint32_t groupCountY = (levelHeight + 32 - 1) / 32;
            vkCmdPushConstants(commandBuffer, cull.depthPyramidPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(glm::vec2), &levelSize);
            vkCmdDispatch(commandBuffer, groupCountX, groupCountY, 1);

            vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &cull.depthPyramidMipLevelBarriers[i]);
        }

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT, VK_DEPENDENCY_BY_REGION_BIT, 0, 0, 0, 0, 1, &cull.framebufferDepthReadBarrier);
    }

    void ArxSwapChain::createBarriers() {
        // Barriers for layout transition between read and write access of the depth pyramid's levels
        cull.depthPyramidMipLevelBarriers.resize(cull.depthPyramidLevels, {});
        for (uint32_t i = 0; i < cull.depthPyramidLevels; ++i) {
            VkImageMemoryBarrier& barrier = cull.depthPyramidMipLevelBarriers[i];
            barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
            barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            barrier.image = cull.depthPyramidImage;
            barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            barrier.subresourceRange.baseMipLevel = i;
            barrier.subresourceRange.levelCount = 1;
            barrier.subresourceRange.baseArrayLayer = 0;
            barrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
        }

        // Barriers for read/write access of the single sampled depth image attached to the framebuffers
        cull.framebufferDepthWriteBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        cull.framebufferDepthWriteBarrier.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        cull.framebufferDepthWriteBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        cull.framebufferDepthWriteBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        cull.framebufferDepthWriteBarrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        cull.framebufferDepthWriteBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        cull.framebufferDepthWriteBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        cull.framebufferDepthWriteBarrier.image = depthImage;
        cull.framebufferDepthWriteBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        cull.framebufferDepthWriteBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        cull.framebufferDepthWriteBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;

        cull.framebufferDepthReadBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        cull.framebufferDepthReadBarrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        cull.framebufferDepthReadBarrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        cull.framebufferDepthReadBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        cull.framebufferDepthReadBarrier.newLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        cull.framebufferDepthReadBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        cull.framebufferDepthReadBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        cull.framebufferDepthReadBarrier.image = depthImage;
        cull.framebufferDepthReadBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        cull.framebufferDepthReadBarrier.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
        cull.framebufferDepthReadBarrier.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    }

    std::vector<uint32_t> ArxSwapChain::computeCulling(VkCommandBuffer commandBuffer, const uint32_t instances, bool late) {
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, late ? cull.lateCullingPipeline->computePipeline : cull.cullingPipeline->computePipeline);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, late ? cull.lateCullingPipelineLayout : cull.cullingPipelineLayout, 0, 1, &cull.cullingDescriptorSet, 0, nullptr);
        
        uint32_t groupCountX = static_cast<uint32_t>((instances / 256) + 1);
        vkCmdDispatch(commandBuffer, groupCountX, 1, 1);
        
        VkBufferMemoryBarrier barrier = {
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT, // From compute shader
                .dstAccessMask = VK_ACCESS_HOST_READ_BIT, // To CPU read
                .buffer = cull.visibilityBuffer->getBuffer(),
                .size = VK_WHOLE_SIZE
            };
        
            vkCmdPipelineBarrier(
                commandBuffer,
                VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, // After compute shader
                VK_PIPELINE_STAGE_HOST_BIT, // Before host read
                0,
                0, nullptr,
                1, &barrier,
                0, nullptr
             );
    
        // Read data directly from the mapped buffer
        uint32_t* ptr = static_cast<uint32_t*>(cull.visibilityBuffer->getMappedMemory());
//        for (size_t i = 0; i < cull.visibleIndices.size(); ++i) {
//                std::cout << "Object " << i << " depth: " << ptr[i] << std::endl;
//        }

        std::vector<uint32_t> ret;
        for (size_t i = 0; i < cull.visibleIndices.size(); ++i) {
            if (ptr[i] == 1) {
                ret.push_back(cull.visibleIndices.indices[i]);
            }
        }
        
        return ret;
    }

    void ArxSwapChain::loadGeometryToDevice() {

        VkMemoryPropertyFlags memoryPropertyFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        
        // ObjectBuffer
        cull.objectsDataBuffer = std::make_unique<ArxBuffer>(
            device,
            sizeof(OcclusionSystem::GPUObjectDataBuffer::GPUObjectData),
            cull.objectData.size(),
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            memoryPropertyFlags);

        cull.objectsDataBuffer->map();
        cull.objectsDataBuffer->writeToBuffer(cull.objectData.dataPtr(), cull.objectData.bufferSize());
        
        // VisibleIndices
        cull.visibilityBuffer = std::make_unique<ArxBuffer>(
            device,
            sizeof(uint32_t),
            cull.visibleIndices.size(),
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        cull.visibilityBuffer->map();
        cull.visibilityBuffer->writeToBuffer(cull.visibleIndices.data());
        
        // Global data
        cull.globalDataBuffer = std::make_unique<ArxBuffer>(
            device,
            sizeof(OcclusionSystem::GPUCullingGlobalData),
            1,
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            memoryPropertyFlags);
        
        cull.globalDataBuffer->map();
        cull.globalDataBuffer->writeToBuffer(&cull.cullingData);
                
        // GPUCameraData
        cull.cameraBuffer = std::make_unique<ArxBuffer>(
                device,
                sizeof(OcclusionSystem::GPUCameraData),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                memoryPropertyFlags);
        
        cull.cameraBuffer->map();
        cull.cameraBuffer->writeToBuffer(&cull.cameraData);
        
        // GPUMiscData
        cull.miscBuffer = std::make_unique<ArxBuffer>(
                device,
                sizeof(OcclusionSystem::GPUMiscData),
                1,
                VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                memoryPropertyFlags);
        
        cull.miscBuffer->map();
        cull.miscBuffer->writeToBuffer(&cull.miscData);
        
        createCullingDescriptors();
    }

    void ArxSwapChain::updateDynamicData() {
        // Update camera
        cull.cameraBuffer->writeToBuffer(&cull.cameraData, static_cast<uint64_t>(sizeof(OcclusionSystem::GPUCameraData)));
        cull.cameraBuffer->flush();
        
        // Update misc data
        cull.miscBuffer->writeToBuffer(&cull.miscData, static_cast<uint64_t>(sizeof(OcclusionSystem::GPUMiscData)));
        cull.miscBuffer->flush();
    }

    VkBufferMemoryBarrier ArxSwapChain::bufferBarrier(VkBuffer buffer, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask)
    {
        VkBufferMemoryBarrier result = { VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER };

        result.srcAccessMask = srcAccessMask;
        result.dstAccessMask = dstAccessMask;
        result.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        result.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        result.buffer = buffer;
        result.offset = 0;
        result.size = VK_WHOLE_SIZE;

        return result;
    }
}
