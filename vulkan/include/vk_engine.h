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

#include <array>
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

    VkQueryPool _timestampQueryPool{VK_NULL_HANDLE};
};

constexpr unsigned int FRAME_OVERLAP = 2;

struct EngineStats
{
    float frameTime;
    int triangleCount;
    int drawcallCount;
    float meshDrawTime;
    float gpuDrawTime{0.f};
    float fenceTime;
    float flushTime;
    float submitTime;
    float presentTime;

    // accumulators for averaging
    float fenceTimeAccum{0.f};
    float flushTimeAccum{0.f};
    float submitTimeAccum{0.f};
    float presentTimeAccum{0.f};
    int sampleCount{0};

    // fps averaging
    float avgFps{0.f};
    uint32_t fpsFrameCount{0};
    std::chrono::high_resolution_clock::time_point fpsWindowStart{};

    // 1% low / 0.1% low fps
    static constexpr int kPercentileWindow{1000};
    // std::array lives on the stack whereas std::vector needs heap allocation
    std::array<float, kPercentileWindow> frameTimeHistory{};
    int frameTimeHistoryIndex{0};
    bool frameTimeHistoryFilled{false};
    float fps1Low{0.f};
    float fps01Low{0.f};

    // frame time graph
    static constexpr int kGraphSize{1000};
    std::array<float, kGraphSize> frameTimeGraph{};
    int frameTimeGraphIndex{0};

    static constexpr int kSampleInterval{30}; // update display every N frames
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

    void init(int initialScene = 0);
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

    // Skybox cubemap
    AllocatedImage _skyboxCubemap{};
    VkSampler _skyboxSampler{VK_NULL_HANDLE};
    VkDescriptorSetLayout _skyboxDescriptorLayout{VK_NULL_HANDLE};
    VkPipeline _skyboxPipeline{VK_NULL_HANDLE};
    VkPipelineLayout _skyboxCubemapPipelineLayout{VK_NULL_HANDLE};

    GPUMeshBuffers _debugRectangle;
    DrawContext _drawCommands;

    Camera _mainCamera;
    DirectionalLight _sunLight;
    SpotlightState _spotlight;

    EngineStats _stats;
    float _timestampPeriod{0.f};

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
    void initSkyboxPipeline();
    void loadSkyboxCubemap(const std::string& dir);
    void destroySkyboxCubemap();
    void drawSkybox(VkCommandBuffer cmd);

    FrameData& getCurrentFrame();
    FrameData& getLastFrame();
};
