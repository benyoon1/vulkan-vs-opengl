// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include "camera.h"
#include "directionalLight.h"
#include "scene.h"
#include "spotlight.h"
#include "vk_context.h"
#include "vk_descriptors.h"
#include "vk_loader.h"
#include "vk_material.h"
#include "vk_resources.h"
#include "vk_swapchain.h"
#include "vk_types.h"
#include <imgui.h>
#include <vk_mem_alloc.h>

#include <chrono>

#include <vector>

struct MeshAsset;
namespace fastgltf
{
struct Mesh;
}

struct ComputePushConstants
{
    glm::mat4 inverseProjection;
    glm::mat4 inverseView;
    glm::vec4 screenSize;
    glm::vec4 sunDirection;
};

struct ComputeEffect
{
    const char* name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputePushConstants data;
};

struct FrameData
{
    VkSemaphore _swapchainSemaphore;
    VkFence _renderFence;

    AllocatedBuffer sceneDataBuffer;
    AllocatedBuffer shadowSceneDataBuffer;

    AllocatedBuffer instanceBuffer;

    DescriptorAllocatorGrowable _frameDescriptors;
    DeletionQueue _deletionQueue;

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
};

constexpr unsigned int FRAME_OVERLAP = 2;

struct EngineStats
{
    float frametime;
    int triangle_count;
    int drawcall_count;
    float mesh_draw_time;
    float fence_time;
    float flush_time;
    float submit_time;
    float present_time;

    // accumulators for averaging
    float fence_time_accum = 0.f;
    float flush_time_accum = 0.f;
    float submit_time_accum = 0.f;
    float present_time_accum = 0.f;
    int sample_count = 0;

    // fps averaging
    float avg_fps{0.f};
    uint32_t fps_frame_count{0};
    std::chrono::high_resolution_clock::time_point fps_window_start{};

    static constexpr int kSampleInterval = 30; // Update display every N frames
};

class VulkanEngine
{
public:
    VulkanContext ctx;
    Swapchain swapchain;
    ResourceManager resources;
    GLTFMetallic_Roughness metalRoughMaterial;
    Scene scene;

    VkDescriptorSetLayout gpuSceneDataDescriptorLayout{VK_NULL_HANDLE};

    void init();
    void run();
    void cleanup();

    // singleton style getter.multiple engines is not supported
    static VulkanEngine& Get();

private:
    bool _isInitialized{false};
    int _frameNumber{0};

    AllocatedBuffer _defaultGLTFMaterialData;

    FrameData _frames[FRAME_OVERLAP];

    VkDescriptorPool _descriptorPool;

    DescriptorAllocator _globalDescriptorAllocator;

    VkPipeline _gradientPipeline;
    VkPipelineLayout _skyboxPipelineLayout;

    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    DeletionQueue _mainDeletionQueue;

    // Shadow map
    AllocatedImage _shadowImage{};
    VkExtent2D _shadowExtent{2048, 2048};
    VkSampler _shadowSampler{VK_NULL_HANDLE};
    TextureID _shadowTexId{};
    VkPipeline _shadowPipeline{VK_NULL_HANDLE};
    VkPipelineLayout _shadowPipelineLayout{VK_NULL_HANDLE};

    // Debug: visualize sunlight position
    GPUMeshBuffers _debugCube{};
    VkPipeline _lightDebugPipeline{VK_NULL_HANDLE};
    VkPipelineLayout _lightDebugPipelineLayout{VK_NULL_HANDLE};
    VkPipeline _textureDebugPipeline{VK_NULL_HANDLE};
    VkPipelineLayout _textureDebugPipelineLayout{VK_NULL_HANDLE};

    VkPipeline _instancedPipeline{VK_NULL_HANDLE};
    VkPipelineLayout _instancedPipelineLayout{VK_NULL_HANDLE};

    GPUMeshBuffers _debugRectangle;
    DrawContext _drawCommands;

    Camera _mainCamera;
    DirectionalLight _sunLight;
    SpotlightState _spotlight;

    EngineStats _stats;

    std::vector<ComputeEffect> _backgroundEffects;
    int _currentBackgroundEffect{0};
    bool resizeRequested{false};
    bool freezeRendering{false};

    void initSwapchain();
    void resizeSwapchain();
    void initCommands();
    void initPipelines();
    void initBackgroundPipelines();
    void initDescriptors();
    void initSyncStructures();
    void initImgui();
    void initDefaultData();

    void updateScene();

    // draw loop
    void draw();
    void drawMain(VkCommandBuffer cmd);
    void drawGeometry(VkCommandBuffer cmd);
    void drawShadowMap(VkCommandBuffer cmd);
    void drawImgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void drawDebugTexture(VkCommandBuffer cmd);

    void initShadowResources();
    void initShadowPipeline();
    void initInstancedPipeline();
    void initLightDebugPipeline();
    void initDebugTexturePipeline();

    FrameData& getCurrentFrame();
    FrameData& getLastFrame();
};
