#include "vk_context.h"

#include "VkBootstrap.h"
#include "vk_initializers.h"

#include <SDL.h>
#include <SDL_vulkan.h>

#include <filesystem>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#ifndef ENABLE_VALIDATION_LAYERS
#define ENABLE_VALIDATION_LAYERS 0
#endif

constexpr bool bUseValidationLayers = ENABLE_VALIDATION_LAYERS;

std::string shader_path(const char* filename)
{
    namespace fs = std::filesystem;
    fs::path from_bin = fs::path("../shaders") / filename; // running from build/<CONFIG>
    if (fs::exists(from_bin))
    {
        return from_bin.string();
    }
    fs::path from_root = fs::path("shaders") / filename; // running from repo root
    if (fs::exists(from_root))
    {
        return from_root.string();
    }
    // Fallback to previous relative path to preserve legacy behavior
    return (fs::path("../../shaders") / filename).string();
}

std::string asset_path(std::string_view relativePath)
{
    namespace fs = std::filesystem;
    fs::path requested(relativePath);

    if (requested.is_absolute())
    {
        return requested.string();
    }

    auto begins_with_assets = [&]()
    {
        auto it = requested.begin();
        return it != requested.end() && *it == fs::path("assets");
    }();

    std::vector<fs::path> candidates;

    if (begins_with_assets)
    {
        candidates.emplace_back(requested);
        candidates.emplace_back(fs::path("..") / requested);
    }
    else
    {
        candidates.emplace_back(fs::path("assets") / requested);
        candidates.emplace_back(fs::path("../assets") / requested);
        candidates.emplace_back(fs::path("../../assets") / requested);
        candidates.emplace_back(fs::path("../../../assets") / requested);
    }

    // Compatibility fallbacks: try without any prefix in current and parent dirs
    candidates.emplace_back(requested);
    candidates.emplace_back(fs::path("..") / requested);

    for (const auto& candidate : candidates)
    {
        if (!candidate.empty() && fs::exists(candidate))
        {
            return candidate.string();
        }
    }

    // If nothing exists, default to the first candidate to preserve path structure
    return candidates.front().string();
}

void VulkanContext::init()
{
    SDL_Init(SDL_INIT_VIDEO);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    window =
        SDL_CreateWindow("Vulkan Renderer", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, windowExtent.width,
                         windowExtent.height, window_flags);

    SDL_ShowCursor(SDL_DISABLE);
    SDL_SetWindowGrab(window, SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_TRUE);

    initVulkan();
    initImmediateSubmit();
}

void VulkanContext::initVulkan()
{
    vkb::InstanceBuilder builder;

    auto inst_ret = builder.set_app_name("Example Vulkan Application")
                        .request_validation_layers(bUseValidationLayers)
                        .use_default_debug_messenger()
                        .require_api_version(1, 3, 0)
                        .build();

    vkb::Instance vkb_inst = inst_ret.value();

    instance = vkb_inst.instance;
    debugMessenger = vkb_inst.debug_messenger;

    SDL_Vulkan_CreateSurface(window, instance, &surface);

    VkPhysicalDeviceVulkan13Features features13{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
    };
    features13.dynamicRendering = VK_TRUE;
    features13.synchronization2 = VK_TRUE;

    VkPhysicalDeviceVulkan12Features features12{
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
    };
    features12.bufferDeviceAddress = VK_TRUE;
    features12.descriptorIndexing = VK_TRUE;
    features12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
    features12.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
    features12.descriptorBindingPartiallyBound = VK_TRUE;
    features12.descriptorBindingVariableDescriptorCount = VK_TRUE;
    features12.runtimeDescriptorArray = VK_TRUE;

    VkPhysicalDeviceFeatures features{};
    features.samplerAnisotropy = VK_TRUE;

    vkb::PhysicalDeviceSelector selector{vkb_inst};
    auto selection_result = selector.set_minimum_version(1, 3)
                                .set_required_features_13(features13)
                                .set_required_features_12(features12)
                                .set_required_features(features)
                                .set_surface(surface)
                                .prefer_gpu_device_type(vkb::PreferredDeviceType::discrete)
                                .select_devices();

    if (!selection_result.has_value() || selection_result->empty())
    {
        fmt::print("Failed to find a suitable GPU!\n");
        abort();
    }

    constexpr uint32_t NVIDIA_VENDOR_ID = 0x10DE;

    vkb::PhysicalDevice physicalDevice = selection_result->front();
    for (auto& dev : selection_result.value())
    {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(dev.physical_device, &props);
        if (props.vendorID == NVIDIA_VENDOR_ID)
        {
            physicalDevice = dev;
            break;
        }
    }

    fmt::print("Selected GPU: {}\n", physicalDevice.name);

    vkb::DeviceBuilder deviceBuilder{physicalDevice};
    vkb::Device vkbDevice = deviceBuilder.build().value();

    device = vkbDevice.device;
    chosenGPU = physicalDevice.physical_device;

    graphicsQueue = vkbDevice.get_queue(vkb::QueueType::graphics).value();
    graphicsQueueFamily = vkbDevice.get_queue_index(vkb::QueueType::graphics).value();

    VmaAllocatorCreateInfo allocatorInfo = {};
    allocatorInfo.physicalDevice = chosenGPU;
    allocatorInfo.device = device;
    allocatorInfo.instance = instance;
    allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
    vmaCreateAllocator(&allocatorInfo, &allocator);
}

void VulkanContext::initImmediateSubmit()
{
    VkCommandPoolCreateInfo commandPoolInfo =
        vkinit::command_pool_create_info(graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

    VK_CHECK(vkCreateCommandPool(device, &commandPoolInfo, nullptr, &_immCommandPool));

    VkCommandBufferAllocateInfo cmdAllocInfo = vkinit::command_buffer_allocate_info(_immCommandPool, 1);
    VK_CHECK(vkAllocateCommandBuffers(device, &cmdAllocInfo, &_immCommandBuffer));

    VkFenceCreateInfo fenceCreateInfo = vkinit::fence_create_info(VK_FENCE_CREATE_SIGNALED_BIT);
    VK_CHECK(vkCreateFence(device, &fenceCreateInfo, nullptr, &_immFence));
}

void VulkanContext::immediateSubmit(std::function<void(VkCommandBuffer cmd)>&& function)
{
    VK_CHECK(vkResetFences(device, 1, &_immFence));
    VK_CHECK(vkResetCommandBuffer(_immCommandBuffer, 0));

    VkCommandBuffer cmd = _immCommandBuffer;
    VkCommandBufferBeginInfo cmdBeginInfo =
        vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

    VK_CHECK(vkBeginCommandBuffer(cmd, &cmdBeginInfo));

    function(cmd);

    VK_CHECK(vkEndCommandBuffer(cmd));

    VkCommandBufferSubmitInfo cmdinfo = vkinit::command_buffer_submit_info(cmd);
    VkSubmitInfo2 submit = vkinit::submit_info(&cmdinfo, nullptr, nullptr);

    VK_CHECK(vkQueueSubmit2(graphicsQueue, 1, &submit, _immFence));

    VK_CHECK(vkWaitForFences(device, 1, &_immFence, true, 9999999999));
}

void VulkanContext::cleanup()
{
    if (_immFence)
        vkDestroyFence(device, _immFence, nullptr);
    if (_immCommandPool)
        vkDestroyCommandPool(device, _immCommandPool, nullptr);

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vmaDestroyAllocator(allocator);
    vkDestroyDevice(device, nullptr);
    vkb::destroy_debug_utils_messenger(instance, debugMessenger);
    vkDestroyInstance(instance, nullptr);

    if (window)
    {
        SDL_ShowCursor(SDL_ENABLE);
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_SetWindowGrab(window, SDL_FALSE);
    }

    SDL_DestroyWindow(window);
}
