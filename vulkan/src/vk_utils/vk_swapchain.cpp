#include "vk_swapchain.h"

#include "VkBootstrap.h"
#include "vk_context.h"
#include "vk_initializers.h"

#include <SDL.h>

void Swapchain::init(VulkanContext& ctx)
{
    create(ctx, ctx.windowExtent.width, ctx.windowExtent.height);

    VkExtent3D drawImageExtent = {ctx.windowExtent.width, ctx.windowExtent.height, 1};

    drawImage.imageFormat = VK_FORMAT_R8G8B8A8_UNORM;
    drawImage.imageExtent = drawImageExtent;

    VkImageUsageFlags drawImageUsages{};
    drawImageUsages |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_STORAGE_BIT;
    drawImageUsages |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    VkImageCreateInfo rimg_info = vkinit::image_create_info(drawImage.imageFormat, drawImageUsages, drawImageExtent);

    VmaAllocationCreateInfo rimg_allocinfo = {};
    rimg_allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    rimg_allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    vmaCreateImage(ctx.allocator, &rimg_info, &rimg_allocinfo, &drawImage.image, &drawImage.allocation, nullptr);

    VkImageViewCreateInfo rview_info =
        vkinit::imageview_create_info(drawImage.imageFormat, drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

    VK_CHECK(vkCreateImageView(ctx.device, &rview_info, nullptr, &drawImage.imageView));

    depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
    depthImage.imageExtent = drawImageExtent;
    VkImageUsageFlags depthImageUsages{};
    depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

    VkImageCreateInfo dimg_info = vkinit::image_create_info(depthImage.imageFormat, depthImageUsages, drawImageExtent);

    vmaCreateImage(ctx.allocator, &dimg_info, &rimg_allocinfo, &depthImage.image, &depthImage.allocation, nullptr);

    VkImageViewCreateInfo dview_info =
        vkinit::imageview_create_info(depthImage.imageFormat, depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

    VK_CHECK(vkCreateImageView(ctx.device, &dview_info, nullptr, &depthImage.imageView));

    _deletionQueue.push_function(
        [&ctx, this]()
        {
            vkDestroyImageView(ctx.device, drawImage.imageView, nullptr);
            vmaDestroyImage(ctx.allocator, drawImage.image, drawImage.allocation);

            vkDestroyImageView(ctx.device, depthImage.imageView, nullptr);
            vmaDestroyImage(ctx.allocator, depthImage.image, depthImage.allocation);
        });
}

void Swapchain::create(VulkanContext& ctx, uint32_t width, uint32_t height)
{
    vkb::SwapchainBuilder swapchainBuilder{ctx.chosenGPU, ctx.device, ctx.surface};

    imageFormat = VK_FORMAT_B8G8R8A8_UNORM;

    vkb::Swapchain vkbSwapchain =
        swapchainBuilder
            .set_desired_format(
                VkSurfaceFormatKHR{.format = imageFormat, .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR})
            .set_desired_present_mode(VK_PRESENT_MODE_IMMEDIATE_KHR)
            .set_desired_extent(width, height)
            .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
            .build()
            .value();

    extent = vkbSwapchain.extent;
    swapchain = vkbSwapchain.swapchain;
    images = vkbSwapchain.get_images().value();
    imageViews = vkbSwapchain.get_image_views().value();

    presentSemaphores.resize(images.size());
    VkSemaphoreCreateInfo semaphoreCreateInfo = vkinit::semaphore_create_info();
    for (size_t i = 0; i < presentSemaphores.size(); ++i)
    {
        VK_CHECK(vkCreateSemaphore(ctx.device, &semaphoreCreateInfo, nullptr, &presentSemaphores[i]));
    }
}

void Swapchain::destroy(VulkanContext& ctx)
{
    vkDestroySwapchainKHR(ctx.device, swapchain, nullptr);

    for (int i = 0; i < imageViews.size(); i++)
    {
        vkDestroyImageView(ctx.device, imageViews[i], nullptr);
    }
    for (VkSemaphore s : presentSemaphores)
    {
        vkDestroySemaphore(ctx.device, s, nullptr);
    }
    presentSemaphores.clear();

    _deletionQueue.flush();
}

void Swapchain::resize(VulkanContext& ctx)
{
    vkDeviceWaitIdle(ctx.device);

    // Only destroy the swapchain part, not the draw/depth images
    vkDestroySwapchainKHR(ctx.device, swapchain, nullptr);
    for (int i = 0; i < imageViews.size(); i++)
    {
        vkDestroyImageView(ctx.device, imageViews[i], nullptr);
    }
    for (VkSemaphore s : presentSemaphores)
    {
        vkDestroySemaphore(ctx.device, s, nullptr);
    }
    presentSemaphores.clear();

    int w, h;
    SDL_GetWindowSize(ctx.window, &w, &h);
    ctx.windowExtent.width = w;
    ctx.windowExtent.height = h;

    create(ctx, ctx.windowExtent.width, ctx.windowExtent.height);
}
