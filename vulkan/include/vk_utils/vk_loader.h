#pragma once

#include "vk_descriptors.h"
#include "vk_types.h"

#include <unordered_map>

class VulkanContext;
class ResourceManager;
struct GLTFMetallic_Roughness;

struct GLTFMaterial
{
    MaterialInstance data;
};

struct GeoSurface
{
    uint32_t startIndex;
    uint32_t count;
    Bounds bounds;
    std::shared_ptr<GLTFMaterial> material;
};

struct MeshAsset
{
    std::string name;

    std::vector<GeoSurface> surfaces;
    GPUMeshBuffers meshBuffers;
};

struct MeshNode : public Node
{
    std::shared_ptr<MeshAsset> mesh;

    virtual void addToDrawCommands(const glm::mat4& topMatrix, DrawContext& ctx) override;
};

struct LoadedGLTF : public IRenderable
{
    // storage for all the data on a given gltf file
    std::unordered_map<std::string, std::shared_ptr<MeshAsset>> meshes;
    std::unordered_map<std::string, std::shared_ptr<Node>> nodes;
    std::unordered_map<std::string, AllocatedImage> images;
    std::unordered_map<std::string, std::shared_ptr<GLTFMaterial>> materials;

    // nodes that dont have a parent, for iterating through the file in tree order
    std::vector<std::shared_ptr<Node>> topNodes;

    std::vector<VkSampler> samplers;

    DescriptorAllocatorGrowable descriptorPool;

    AllocatedBuffer materialDataBuffer;

    VulkanContext* creatorCtx;
    ResourceManager* creatorResources;

    ~LoadedGLTF() { clearAll(); };

    virtual void addToDrawCommands(const glm::mat4& topMatrix, DrawContext& ctx);

private:
    void clearAll();
};

std::optional<std::shared_ptr<LoadedGLTF>> loadGltf(VulkanContext& ctx, ResourceManager& resources,
                                                     GLTFMetallic_Roughness& material, std::string_view filePath);
std::optional<std::shared_ptr<LoadedGLTF>> loadAssimpAssets(VulkanContext& ctx, ResourceManager& resources,
                                                            GLTFMetallic_Roughness& material,
                                                            std::string_view filePath);
