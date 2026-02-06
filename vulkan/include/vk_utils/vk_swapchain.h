#pragma once

#include "vk_types.h"

#include <vector>

class VulkanContext;

class Swapchain
{
public:
    VkSwapchainKHR swapchain{VK_NULL_HANDLE};
    VkFormat imageFormat{};
    VkExtent2D extent{};
    VkExtent2D drawExtent{};

    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    std::vector<VkSemaphore> presentSemaphores;

    AllocatedImage drawImage{};
    AllocatedImage depthImage{};

    void init(VulkanContext& ctx);
    void create(VulkanContext& ctx, uint32_t width, uint32_t height);
    void destroy(VulkanContext& ctx);
    void resize(VulkanContext& ctx);

private:
    DeletionQueue _deletionQueue;
};
