#ifndef APPLICATION_H
#define APPLICATION_H

#include "core/window.h"
#include "render/shader.h"
#include "render/shadowMap.h"
#include "scene/camera.h"
#include "scene/lights/directionalLight.h"
#include "scene/lights/spotlight.h"
#include "scene/model.h"
#include "scene/skybox.h"
#include "scene/sphere.h"
#include <chrono>
#include <imgui.h>
#include <memory>
#include <string>
#include <vector>

struct EngineStats
{
    float frameTime{0.f};
    float meshDrawTime{0.f};
    int triangleCount{0};
    int drawcallCount{0};
};

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
    glm::vec3 cameraStartPos;
    glm::vec3 sunStartPos;
};

class Application
{
public:
    static constexpr ImS32 kSliderMin{0};
    static constexpr ImS32 kSliderMax{30000};
    static constexpr float kRotationSpeed{10.f};

    Application(int initialScene = 0);
    ~Application();

    ImS32 numAsteroids{15000};
    bool useInstancing{false};
    float deltaTime{0.0f};

    void run();
    void updateFrame();
    void update();
    void renderDepthPass();
    void renderMainPass();
    void renderImGui();
    void loadScene(int index);

private:
    Window m_window;
    Camera m_camera;
    DirectionalLight m_sunLight;
    Spotlight m_spotlight;
    float m_sunSpeed{0.1f};
    float m_spotlightGain{1.0f};
    double m_swapTime{0.0};
    int m_fpsFrameCount{0};
    float m_avgFps{0.0f};
    float _asteroidTime{0.0f};
    EngineStats m_stats{};

    // asteroid belt parameters
    float _majorRadius{25.0f};  // distance from center to the inside of tube
    float _minorRadius{4.0f};   // tube radius (belt thickness)
    float _verticalScale{0.3f}; // make the belt thin vertically
    float _minScale{0.02f};     // min asteroid size
    float _maxScale{0.07f};     // max asteroid size

    float m_currentFrame{0.0f};
    float m_lastFrame{0.0f};

    std::chrono::high_resolution_clock::time_point m_fpsWindowStart{};

    std::vector<SceneEntry> m_sceneRegistry;
    int m_currentSceneIndex{0};

    // GL-dependent resources are deferred
    std::unique_ptr<Model> m_icosahedron;
    std::unique_ptr<Model> m_planet;
    std::unique_ptr<Model> m_bistro;
    std::unique_ptr<Skybox> m_skybox;
    std::unique_ptr<ShadowMap> m_sunShadow;
    std::unique_ptr<ShadowMap> m_spotShadow;
    std::unique_ptr<Shader> m_modelShader;
    std::unique_ptr<Shader> m_skyboxShader;
    std::unique_ptr<Shader> m_depthShader;
    std::unique_ptr<Shader> m_instancedModelShader;
    std::unique_ptr<Shader> m_instancedDepthShader;

    std::vector<glm::mat4> m_asteroidTransforms;
};

#endif // APPLICATION_H