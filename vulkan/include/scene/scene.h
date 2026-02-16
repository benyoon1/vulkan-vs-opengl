#pragma once

#include "camera.h"
#include "directionalLight.h"
#include "vk_loader.h"
#include "vk_types.h"

#include <glm/ext/vector_float3.hpp>
#include <imgui.h>

#include <string>
#include <unordered_map>
#include <vector>

class ResourceManager;
class VulkanContext;
struct GLTFMetallic_Roughness;

enum class SceneType
{
    PlanetAndAsteroids,
    AmazonBistro,
};

struct SceneEntry
{
    std::string name;
    std::string assetPath;
    SceneType type;
    float scale{1.0f};
    glm::vec3 cameraStartPos{0.0f, 0.0f, 0.0f};
    glm::vec3 sunStartPos{0.0f, 0.0f, 0.0f};
};

struct InstancedMeshInfo
{
    uint32_t indexCount{0};
    uint32_t firstIndex{0};
    VkBuffer indexBuffer{VK_NULL_HANDLE};
    MaterialInstance* material{nullptr};
    VkDeviceAddress vertexBufferAddress{0};
};

class Scene
{
public:
    static constexpr ImS32 kSliderMin{0};
    static constexpr ImS32 kSliderMax{30000};
    static constexpr float kRotationSpeed{10.f};

    GPUSceneData sceneData{};

    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedAssets;

    // asteroid belt parameters
    ImS32 numAsteroids{15000};
    float majorRadius{25.0f};  // distance from center to the inside of tube
    float minorRadius{4.0f};   // tube radius (belt thickness)
    float verticalScale{0.3f}; // make the belt thin vertically
    float minScale{0.02f};     // min asteroid size
    float maxScale{0.07f};     // max asteroid size

    // instancing
    bool useInstancing{false};
    std::vector<glm::mat4> asteroidTransforms;
    InstancedMeshInfo instancedMeshInfo;

    // scene selection
    std::vector<SceneEntry> sceneRegistry;
    int currentSceneIndex{0};

    void initRenderables(VulkanContext& ctx, ResourceManager& resources, GLTFMetallic_Roughness& material,
                         Camera& camera, DirectionalLight& sunLight);
    void loadScene(int index);
    void update(VkExtent2D& windowExtent, DrawContext& drawCommands, Camera& mainCamera, DirectionalLight& sunLight);
    void processSliderEvent();
    void updateFrame();
    void cleanup();

private:
    float _asteroidTime{0.0f};

    // delta time for consistency regardless of fps
    float _deltaTime{0.0f};
    float _currentFrame{0.0f};
    float _lastFrame{0.0f};

    std::shared_ptr<MeshAsset> _icosahedronMesh;

    // stored references for scene reloading
    VulkanContext* _ctx{nullptr};
    ResourceManager* _resources{nullptr};
    GLTFMetallic_Roughness* _material{nullptr};
    Camera* _camera{nullptr};
    DirectionalLight* _sunLight{nullptr};

    void loadPlanetAndAsteroids(const std::string& assetPath);
    void loadAmazonBistro(const std::string& assetPath);
    void updatePlanetAndAsteroids(DrawContext& drawCommands);
    void updateAmazonBistro(DrawContext& drawCommands);
};
