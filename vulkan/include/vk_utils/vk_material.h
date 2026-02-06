#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"

struct GLTFMetallic_Roughness
{
    MaterialPipeline opaquePipeline;
    MaterialPipeline transparentPipeline;

    VkDescriptorSetLayout materialLayout;

    struct MaterialConstants
    {
        glm::vec4 colorFactors;
        glm::vec4 metal_rough_factors;
        // padding, we need it anyway for uniform buffers
        uint32_t colorTexID;
        uint32_t metalRoughTexID;
        uint32_t pad1;
        uint32_t pad2;
        glm::vec4 extra[13];
    };

    struct MaterialResources
    {
        AllocatedImage colorImage;
        VkSampler colorSampler;
        AllocatedImage metalRoughImage;
        VkSampler metalRoughSampler;
        VkBuffer dataBuffer;
        uint32_t dataBufferOffset;
    };

    DescriptorWriter writer;

    void build_pipelines(VkDevice device, VkDescriptorSetLayout sceneDataLayout, VkFormat drawImageFormat,
                         VkFormat depthImageFormat);
    void clear_resources(VkDevice device);

    MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources,
                                    DescriptorAllocatorGrowable& descriptorAllocator);
};
