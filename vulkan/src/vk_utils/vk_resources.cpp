#include "vk_resources.h"

#include "vk_context.h"
#include "vk_images.h"
#include "vk_initializers.h"

#include <glm/packing.hpp>

void ResourceManager::init(VulkanContext& ctx)
{
    // 3 default textures, white, grey, black. 1 pixel each
    uint32_t white = glm::packUnorm4x8(glm::vec4(1, 1, 1, 1));
    whiteImage =
        createImage(ctx, (void*)&white, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t grey = glm::packUnorm4x8(glm::vec4(0.66f, 0.66f, 0.66f, 1));
    greyImage =
        createImage(ctx, (void*)&grey, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    uint32_t black = glm::packUnorm4x8(glm::vec4(0, 0, 0, 0));
    blackImage =
        createImage(ctx, (void*)&black, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    // checkerboard image
    uint32_t magenta = glm::packUnorm4x8(glm::vec4(1, 0, 1, 1));
    std::array<uint32_t, 16 * 16> pixels;
    for (int x = 0; x < 16; x++)
    {
        for (int y = 0; y < 16; y++)
        {
            pixels[y * 16 + x] = ((x % 2) ^ (y % 2)) ? magenta : black;
        }
    }

    errorCheckerboardImage =
        createImage(ctx, pixels.data(), VkExtent3D{16, 16, 1}, VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_SAMPLED_BIT);

    // Create default samplers with mipmap filtering; anisotropy enabled if supported
    VkPhysicalDeviceProperties props{};
    vkGetPhysicalDeviceProperties(ctx.chosenGPU, &props);

    VkSamplerCreateInfo sampl = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    sampl.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampl.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampl.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    sampl.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampl.minLod = 0.0f;
    sampl.maxLod = VK_LOD_CLAMP_NONE;
    sampl.mipLodBias = 0.0f;

    // sampl.anisotropyEnable = VK_TRUE;
    // sampl.maxAnisotropy = props.limits.maxSamplerAnisotropy > 0 ? props.limits.maxSamplerAnisotropy : 1.0f;

    // Nearest sampler
    sampl.magFilter = VK_FILTER_NEAREST;
    sampl.minFilter = VK_FILTER_NEAREST;
    vkCreateSampler(ctx.device, &sampl, nullptr, &defaultSamplerNearest);

    // Linear sampler
    sampl.magFilter = VK_FILTER_LINEAR;
    sampl.minFilter = VK_FILTER_LINEAR;
    vkCreateSampler(ctx.device, &sampl, nullptr, &defaultSamplerLinear);

    TextureID defaultTextureId = texCache.AddTexture(whiteImage.imageView, defaultSamplerLinear);
    texCache.set_fallback(defaultTextureId);
}

void ResourceManager::cleanup(VulkanContext& ctx)
{
    if (whiteImage.image != VK_NULL_HANDLE)
    {
        destroyImage(ctx, whiteImage);
        whiteImage = {};
    }
    if (greyImage.image != VK_NULL_HANDLE)
    {
        destroyImage(ctx, greyImage);
        greyImage = {};
    }
    if (blackImage.image != VK_NULL_HANDLE)
    {
        destroyImage(ctx, blackImage);
        blackImage = {};
    }
    if (errorCheckerboardImage.image != VK_NULL_HANDLE)
    {
        destroyImage(ctx, errorCheckerboardImage);
        errorCheckerboardImage = {};
    }

    if (defaultSamplerLinear != VK_NULL_HANDLE)
    {
        vkDestroySampler(ctx.device, defaultSamplerLinear, nullptr);
        defaultSamplerLinear = VK_NULL_HANDLE;
    }
    if (defaultSamplerNearest != VK_NULL_HANDLE)
    {
        vkDestroySampler(ctx.device, defaultSamplerNearest, nullptr);
        defaultSamplerNearest = VK_NULL_HANDLE;
    }
}

AllocatedBuffer ResourceManager::createBuffer(VulkanContext& ctx, size_t allocSize, VkBufferUsageFlags usage,
                                              VmaMemoryUsage memoryUsage)
{
    VkBufferCreateInfo bufferInfo = {};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.pNext = nullptr;
    bufferInfo.size = allocSize;
    bufferInfo.usage = usage;

    VmaAllocationCreateInfo vmaallocInfo = {};
    vmaallocInfo.usage = memoryUsage;
    vmaallocInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
    AllocatedBuffer newBuffer;

    VK_CHECK(vmaCreateBuffer(ctx.allocator, &bufferInfo, &vmaallocInfo, &newBuffer.buffer, &newBuffer.allocation,
                             &newBuffer.info));

    return newBuffer;
}

AllocatedImage ResourceManager::createImage(VulkanContext& ctx, VkExtent3D size, VkFormat format,
                                            VkImageUsageFlags usage, bool mipmapped)
{
    AllocatedImage newImage;
    newImage.imageFormat = format;
    newImage.imageExtent = size;

    VkImageCreateInfo img_info = vkinit::image_create_info(format, usage, size);
    if (mipmapped)
    {
        img_info.mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(size.width, size.height)))) + 1;
    }

    VmaAllocationCreateInfo allocinfo = {};
    allocinfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
    allocinfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VK_CHECK(vmaCreateImage(ctx.allocator, &img_info, &allocinfo, &newImage.image, &newImage.allocation, nullptr));

    VkImageAspectFlags aspectFlag = VK_IMAGE_ASPECT_COLOR_BIT;
    if (format == VK_FORMAT_D32_SFLOAT)
    {
        aspectFlag = VK_IMAGE_ASPECT_DEPTH_BIT;
    }

    VkImageViewCreateInfo view_info = vkinit::imageview_create_info(format, newImage.image, aspectFlag);
    view_info.subresourceRange.levelCount = img_info.mipLevels;

    VK_CHECK(vkCreateImageView(ctx.device, &view_info, nullptr, &newImage.imageView));

    return newImage;
}

AllocatedImage ResourceManager::createImage(VulkanContext& ctx, void* data, VkExtent3D size, VkFormat format,
                                            VkImageUsageFlags usage, bool mipmapped)
{
    size_t data_size = size.depth * size.width * size.height * 4;
    AllocatedBuffer uploadbuffer =
        createBuffer(ctx, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

    memcpy(uploadbuffer.info.pMappedData, data, data_size);

    AllocatedImage new_image = createImage(
        ctx, size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped);

    ctx.immediateSubmit(
        [&](VkCommandBuffer cmd)
        {
            vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_UNDEFINED,
                                     VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

            VkBufferImageCopy copyRegion = {};
            copyRegion.bufferOffset = 0;
            copyRegion.bufferRowLength = 0;
            copyRegion.bufferImageHeight = 0;

            copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copyRegion.imageSubresource.mipLevel = 0;
            copyRegion.imageSubresource.baseArrayLayer = 0;
            copyRegion.imageSubresource.layerCount = 1;
            copyRegion.imageExtent = size;

            vkCmdCopyBufferToImage(cmd, uploadbuffer.buffer, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                                   &copyRegion);

            if (mipmapped)
            {
                vkutil::generate_mipmaps(cmd, new_image.image,
                                         VkExtent2D{new_image.imageExtent.width, new_image.imageExtent.height});
            }
            else
            {
                vkutil::transition_image(cmd, new_image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                         VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
        });
    destroyBuffer(ctx, uploadbuffer);
    return new_image;
}

GPUMeshBuffers ResourceManager::uploadMesh(VulkanContext& ctx, std::span<uint32_t> indices, std::span<Vertex> vertices)
{
    const size_t vertexBufferSize = vertices.size() * sizeof(Vertex);
    const size_t indexBufferSize = indices.size() * sizeof(uint32_t);

    GPUMeshBuffers newSurface;

    newSurface.vertexBuffer = createBuffer(ctx, vertexBufferSize,
                                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT |
                                               VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
                                           VMA_MEMORY_USAGE_GPU_ONLY);

    VkBufferDeviceAddressInfo deviceAdressInfo{.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
                                               .buffer = newSurface.vertexBuffer.buffer};
    newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(ctx.device, &deviceAdressInfo);

    newSurface.indexBuffer =
        createBuffer(ctx, indexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                     VMA_MEMORY_USAGE_GPU_ONLY);

    AllocatedBuffer staging = createBuffer(ctx, vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                                           VMA_MEMORY_USAGE_CPU_ONLY);

    void* data = staging.info.pMappedData;

    memcpy(data, vertices.data(), vertexBufferSize);
    memcpy((char*)data + vertexBufferSize, indices.data(), indexBufferSize);

    ctx.immediateSubmit(
        [&](VkCommandBuffer cmd)
        {
            VkBufferCopy vertexCopy{0};
            vertexCopy.dstOffset = 0;
            vertexCopy.srcOffset = 0;
            vertexCopy.size = vertexBufferSize;

            vkCmdCopyBuffer(cmd, staging.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);

            VkBufferCopy indexCopy{0};
            indexCopy.dstOffset = 0;
            indexCopy.srcOffset = vertexBufferSize;
            indexCopy.size = indexBufferSize;

            vkCmdCopyBuffer(cmd, staging.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
        });

    destroyBuffer(ctx, staging);

    return newSurface;
}

void ResourceManager::destroyImage(VulkanContext& ctx, const AllocatedImage& img)
{
    vkDestroyImageView(ctx.device, img.imageView, nullptr);
    vmaDestroyImage(ctx.allocator, img.image, img.allocation);
}

void ResourceManager::destroyBuffer(VulkanContext& ctx, const AllocatedBuffer& buffer)
{
    vmaDestroyBuffer(ctx.allocator, buffer.buffer, buffer.allocation);
}

TextureID TextureCache::AddTexture(const VkImageView& image, VkSampler sampler)
{
    for (unsigned int i = 0; i < Cache.size(); i++)
    {
        if (Cache[i].imageView == image && Cache[i].sampler == sampler)
        {
            return TextureID{i};
        }
    }

    const bool limitActive = maxDescriptors != std::numeric_limits<uint32_t>::max() && maxDescriptors > 0;

    if (limitActive && Cache.size() >= maxDescriptors)
    {
        if (!limitWarningEmitted)
        {
            fmt::print("Texture cache reached capacity ({}). Reusing fallback texture {}.\n", maxDescriptors,
                       fallbackTexture.Index);
            limitWarningEmitted = true;
        }

        if (!Cache.empty() && fallbackTexture.Index < Cache.size())
        {
            return fallbackTexture;
        }

        return TextureID{Cache.empty() ? 0u : static_cast<uint32_t>(Cache.size() - 1)};
    }

    uint32_t idx = Cache.size();

    Cache.push_back(VkDescriptorImageInfo{
        .sampler = sampler, .imageView = image, .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL});

    if (fallbackTexture.Index >= Cache.size())
    {
        fallbackTexture.Index = idx;
    }

    return TextureID{idx};
}
