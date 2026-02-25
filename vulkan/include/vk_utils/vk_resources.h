#pragma once

#include "vk_types.h"

#include <limits>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

class VulkanContext;

struct TextureID
{
    uint32_t Index;
};

struct TextureCache
{
    std::vector<VkDescriptorImageInfo> Cache;
    std::unordered_map<std::string, TextureID> NameMap;
    uint32_t maxDescriptors = std::numeric_limits<uint32_t>::max();
    bool limitWarningEmitted = false;
    TextureID fallbackTexture{0};
    uint32_t engineBaselineCount{0};

    void set_max(uint32_t max)
    {
        maxDescriptors = max;
        limitWarningEmitted = false;
    }

    void set_fallback(TextureID id) { fallbackTexture = id; }

    // mark the current cache size as the engine baseline.
    // clearSceneTextures() will truncate back to this point.
    void markEngineBaseline() { engineBaselineCount = static_cast<uint32_t>(Cache.size()); }

    // remove all textures added after the engine baseline (scene textures).
    void clearSceneTextures()
    {
        if (engineBaselineCount < Cache.size())
        {
            Cache.resize(engineBaselineCount);
        }
        NameMap.clear();
    }

    TextureID AddTexture(const VkImageView& image, VkSampler sampler);
};

class ResourceManager
{
public:
    AllocatedImage whiteImage{};
    AllocatedImage errorCheckerboardImage{};
    AllocatedImage blackImage{};
    AllocatedImage greyImage{};

    VkSampler defaultSamplerLinear{VK_NULL_HANDLE};
    VkSampler defaultSamplerNearest{VK_NULL_HANDLE};

    AllocatedBuffer defaultGLTFMaterialData{};

    TextureCache texCache;

    void init(VulkanContext& ctx);
    void cleanup(VulkanContext& ctx);

    AllocatedBuffer createBuffer(VulkanContext& ctx, size_t allocSize, VkBufferUsageFlags usage,
                                 VmaMemoryUsage memoryUsage);
    AllocatedImage createImage(VulkanContext& ctx, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                               bool mipmapped = false);
    AllocatedImage createImage(VulkanContext& ctx, void* data, VkExtent3D size, VkFormat format,
                               VkImageUsageFlags usage, bool mipmapped = false);
    GPUMeshBuffers uploadMesh(VulkanContext& ctx, std::span<uint32_t> indices, std::span<Vertex> vertices);

    void destroyImage(VulkanContext& ctx, const AllocatedImage& img);
    void destroyBuffer(VulkanContext& ctx, const AllocatedBuffer& buffer);
};
