#pragma once

#include "camera.h"
#include "directionalLight.h"
#include "spotlight.h"
#include "vk_loader.h"
#include "vk_types.h"

#include <imgui.h>

#include <string>
#include <unordered_map>

class ResourceManager;
class VulkanContext;
struct GLTFMetallic_Roughness;

class Scene
{
public:
    static constexpr ImS32 kSliderMin{0};
    static constexpr ImS32 kSliderMax{30000};

    Camera mainCamera;
    DirectionalLight sunLight;
    SpotlightState spotlight;

    DrawContext drawCommands;
    GPUSceneData sceneData{};

    std::unordered_map<std::string, std::shared_ptr<LoadedGLTF>> loadedAssets;

    // asteroid belt parameters
    ImS32 numAsteroids{15000};
    float majorRadius{35.0f};
    float minorRadius{7.5f};
    float verticalScale{0.3f};
    float minScale{0.02f};
    float maxScale{0.07f};

    void initRenderables(VulkanContext& ctx, ResourceManager& resources, GLTFMetallic_Roughness& material);
    void update(float deltaTime, VkExtent2D windowExtent);
    void processSliderEvent(float deltaTime);
    void cleanup();

private:
    float _asteroidTime{0.0f};
};
