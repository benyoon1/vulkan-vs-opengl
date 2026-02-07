// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.
//> intro
#pragma once

#include "vk_mem_alloc.h"

#include <vulkan/vk_enum_string_helper.h>
#include <vulkan/vulkan.h>

#include <fmt/core.h>
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <deque>
#include <functional>
#include <memory>
#include <vector>
//< intro

// we will add our main reusable types here
struct AllocatedImage
{
    VkImage image;
    VkImageView imageView;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct AllocatedBuffer
{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct GPUGLTFMaterial
{
    glm::vec4 colorFactors;
    glm::vec4 metal_rough_factors;
    glm::vec4 extra[14];
};

static_assert(sizeof(GPUGLTFMaterial) == 256);

struct GPUSceneData
{
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 viewproj;
    glm::vec4 cameraPosition;
    glm::vec4 ambientColor;
    glm::vec4 sunlightPosition;
    glm::vec4 sunlightDirection; // w for sun power
    glm::vec4 sunlightColor;
    glm::mat4 sunlightViewProj;
    glm::uvec4 shadowParams;
    glm::vec4 spotlightPos;
    glm::vec4 spotlightDir;
    glm::vec4 spotColor;
    glm::vec4 spotCutoffAndIntensity;
};

//> mat_types
enum class MaterialPass : uint8_t
{
    MainColor,
    Transparent,
    Other
};
struct MaterialPipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

struct MaterialInstance
{
    MaterialPipeline* pipeline;
    VkDescriptorSet materialSet;
    MaterialPass passType;
};
//< mat_types
//> vbuf_types
struct Vertex
{
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

// holds the resources needed for a mesh
struct GPUMeshBuffers
{

    AllocatedBuffer indexBuffer;
    AllocatedBuffer vertexBuffer;
    VkDeviceAddress vertexBufferAddress;
};

// push constants for our mesh object draws
struct GPUDrawPushConstants
{
    glm::mat4 worldMatrix;
    glm::mat4 viewProj;
    VkDeviceAddress vertexBuffer;
};

struct GPUInstancedPushConstants
{
    glm::mat4 viewProj;
    VkDeviceAddress vertexBuffer;
    VkDeviceAddress instanceBuffer;
};
//< vbuf_types

struct DeletionQueue
{
    std::deque<std::function<void()>> deletors;

    void push_function(std::function<void()>&& function) { deletors.push_back(function); }

    void flush()
    {
        // reverse iterate the deletion queue to execute all the functions
        for (auto it = deletors.rbegin(); it != deletors.rend(); it++)
        {
            (*it)(); // call functors
        }

        deletors.clear();
    }
};

struct Bounds
{
    glm::vec3 origin;
    float sphereRadius;
    glm::vec3 extents;
};

struct RenderObject
{
    uint32_t indexCount;
    uint32_t firstIndex;
    VkBuffer indexBuffer;

    MaterialInstance* material;
    Bounds bounds;
    glm::mat4 transform;
    glm::mat4 viewProj;
    VkDeviceAddress vertexBufferAddress;
};

struct DrawContext
{
    std::vector<RenderObject> OpaqueSurfaces;
    std::vector<RenderObject> TransparentSurfaces;
    glm::mat4 viewProj{1.0f};
};

//> node_types
// base class for a renderable dynamic object
class IRenderable
{

    virtual void addToDrawCommands(const glm::mat4& topMatrix, DrawContext& ctx) = 0;
};

// implementation of a drawable scene node.
// the scene node can hold children and will also keep a transform to propagate
// to them
struct Node : public IRenderable
{

    // parent pointer must be a weak pointer to avoid circular dependencies
    std::weak_ptr<Node> parent;
    std::vector<std::shared_ptr<Node>> children;

    glm::mat4 localTransform;
    glm::mat4 worldTransform;

    void refreshTransform(const glm::mat4& parentMatrix)
    {
        worldTransform = parentMatrix * localTransform;
        for (auto c : children)
        {
            c->refreshTransform(worldTransform);
        }
    }

    virtual void addToDrawCommands(const glm::mat4& topMatrix, DrawContext& ctx)
    {
        // draw children
        for (auto& c : children)
        {
            c->addToDrawCommands(topMatrix, ctx);
        }
    }
};
//< node_types
//> intro
#define VK_CHECK(x)                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        VkResult err = x;                                                                                              \
        if (err)                                                                                                       \
        {                                                                                                              \
            fmt::print("Detected Vulkan error: {}", string_VkResult(err));                                             \
            abort();                                                                                                   \
        }                                                                                                              \
    } while (0)
//< intro
