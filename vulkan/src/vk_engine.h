// vulkan_guide.h : Include file for standard system include files,
// or project specific include files.

#pragma once

#include <chrono>
#include <vk_types.h>

#include <deque>
#include <functional>
#include <limits>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <vk_mem_alloc.h>

#include "scene/robotArm.h"
#include "scene/spotlight.h"
#include <camera.h>
#include <scene/directionalLight.h>
#include <vk_descriptors.h>
#include <vk_loader.h>
#include <vk_pipelines.h>

struct MeshAsset;
namespace fastgltf
{
struct Mesh;
}

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

struct FrameData
{
    VkSemaphore _swapchainSemaphore;
    VkFence _renderFence;

    AllocatedBuffer sceneDataBuffer;
    AllocatedBuffer shadowSceneDataBuffer;

    DescriptorAllocatorGrowable _frameDescriptors;
    DeletionQueue _deletionQueue;

    VkCommandPool _commandPool;
    VkCommandBuffer _mainCommandBuffer;
};

constexpr unsigned int FRAME_OVERLAP = 2;

struct DrawContext
{
    std::vector<RenderObject> OpaqueSurfaces;
    std::vector<RenderObject> TransparentSurfaces;
    glm::mat4 viewProj{1.0f};
};

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

    void build_pipelines(VulkanEngine* engine);
    void clear_resources(VkDevice device);

    MaterialInstance write_material(VkDevice device, MaterialPass pass, const MaterialResources& resources,
                                    DescriptorAllocatorGrowable& descriptorAllocator);
};

struct MeshNode : public Node
{

    std::shared_ptr<MeshAsset> mesh;

    virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) override;
};
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

    void set_max(uint32_t max)
    {
        maxDescriptors = max;
        limitWarningEmitted = false;
    }

    void set_fallback(TextureID id) { fallbackTexture = id; }

    TextureID AddTexture(const VkImageView& image, VkSampler sampler);
};

class VulkanEngine
{
public:
    bool _isInitialized{false};
    int _frameNumber{0};

    VkExtent2D _windowExtent{1920, 1080};

    struct SDL_Window* _window{nullptr};

    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debug_messenger;
    VkPhysicalDevice _chosenGPU;
    VkDevice _device;

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    AllocatedBuffer _defaultGLTFMaterialData;

    FrameData _frames[FRAME_OVERLAP];

    VkSurfaceKHR _surface;
    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;
    VkExtent2D _swapchainExtent;
    VkExtent2D _drawExtent;
    VkDescriptorPool _descriptorPool;

    DescriptorAllocator globalDescriptorAllocator;

    VkPipeline _gradientPipeline;
    VkPipelineLayout _skyboxPipelineLayout;

    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;
    // One render-finished semaphore per swapchain image (used for present)
    std::vector<VkSemaphore> _presentSemaphores;

    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    DeletionQueue _mainDeletionQueue;

    VmaAllocator _allocator; // vma lib allocator

    VkDescriptorSetLayout _gpuSceneDataDescriptorLayout;

    GLTFMetallic_Roughness metalRoughMaterial;

    // draw resources
    AllocatedImage _drawImage;
    AllocatedImage _depthImage;

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

    // immediate submit structures
    VkFence _immFence;
    VkCommandBuffer _immCommandBuffer;
    VkCommandPool _immCommandPool;

    AllocatedImage _whiteImage;
    AllocatedImage _blackImage;
    AllocatedImage _greyImage;
    AllocatedImage _errorCheckerboardImage;

    VkSampler _defaultSamplerLinear;
    VkSampler _defaultSamplerNearest;

    TextureCache texCache;
    uint32_t _maxSampledImageDescriptors{0};

    GPUMeshBuffers _debugRectangle;
    DrawContext _drawCommands;

    GPUSceneData _sceneData;

    Camera _mainCamera;
    DirectionalLight _sunLight;
    SpotlightState _spotlight;
    RobotArm _robotArm;
    float _asteroidTime{0.0f};

    EngineStats stats;

    std::vector<ComputeEffect> backgroundEffects;
    int currentBackgroundEffect{0};

    // singleton style getter.multiple engines is not supported
    static VulkanEngine& Get();

    // initializes everything in the engine
    void init();

    // shuts down the engine
    void cleanup();

    // draw loop
    void draw();
    void draw_main(VkCommandBuffer cmd);
    void draw_geometry(VkCommandBuffer cmd);
    void draw_shadow_map(VkCommandBuffer cmd);
    void draw_imgui(VkCommandBuffer cmd, VkImageView targetImageView);
    void draw_debug_texture(VkCommandBuffer cmd);

    void init_shadow_resources();
    void init_shadow_pipeline();
    void init_light_debug_pipeline();
    void init_debug_texture_pipeline();

    void render_nodes();

    // run main loop
    void run();

    void update_scene();

    // upload a mesh into a pair of gpu buffers. If descriptor allocator is not
    // null, it will also create a descriptor that points to the vertex buffer
    GPUMeshBuffers uploadMesh(std::span<uint32_t> indices, std::span<Vertex> vertices);

    FrameData& get_current_frame();
    FrameData& get_last_frame();

    AllocatedBuffer create_buffer(size_t allocSize, VkBufferUsageFlags usage, VmaMemoryUsage memoryUsage);

    AllocatedImage create_image(VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped = false);
    AllocatedImage create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage,
                                bool mipmapped = false);

    void immediate_submit(std::function<void(VkCommandBuffer cmd)>&& function);

    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedAssets;
    std::vector<std::shared_ptr<LoadedGLTF>> brickadiaScene;

    void destroy_image(const AllocatedImage& img);
    void destroy_buffer(const AllocatedBuffer& buffer);

    bool resize_requested{false};
    bool freeze_rendering{false};

private:
    void init_vulkan();

    void init_swapchain();

    void create_swapchain(uint32_t width, uint32_t height);

    void resize_swapchain();

    void destroy_swapchain();

    void init_commands();

    void init_pipelines();
    void init_background_pipelines();

    void init_descriptors();

    void init_sync_structures();

    void init_renderables();

    void init_imgui();

    void init_default_data();
};
