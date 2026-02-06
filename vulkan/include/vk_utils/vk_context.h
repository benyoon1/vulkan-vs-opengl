#pragma once

#include "vk_types.h"

#include <functional>

struct SDL_Window;

class VulkanContext
{
public:
    VkDevice device{VK_NULL_HANDLE};
    VkPhysicalDevice chosenGPU{VK_NULL_HANDLE};
    VkInstance instance{VK_NULL_HANDLE};
    VkDebugUtilsMessengerEXT debugMessenger{VK_NULL_HANDLE};
    VkQueue graphicsQueue{VK_NULL_HANDLE};
    uint32_t graphicsQueueFamily{0};
    VmaAllocator allocator{nullptr};
    VkSurfaceKHR surface{VK_NULL_HANDLE};
    SDL_Window* window{nullptr};
    VkExtent2D windowExtent{1920, 1080};
    uint32_t maxSampledImageDescriptors{0};

    void init();
    void cleanup();

    void immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function);

private:
    VkFence _immFence{VK_NULL_HANDLE};
    VkCommandBuffer _immCommandBuffer{VK_NULL_HANDLE};
    VkCommandPool _immCommandPool{VK_NULL_HANDLE};

    void initVulkan();
    void initImmediateSubmit();
};

// Resolve shader/asset paths relative to cwd (works from build dir or repo root)
std::string shader_path(const char* filename);
std::string asset_path(std::string_view relativePath);
