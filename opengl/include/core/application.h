#ifndef APPLICATION_H
#define APPLICATION_H

#include "core/window.h"
#include "render/shader.h"
#include "render/shadowMap.h"
#include "scene/camera.h"
#include "scene/lights/directionalLight.h"
#include "scene/lights/spotlight.h"
#include "scene/model.h"
#include "scene/robotArm.h"
#include "scene/skybox.h"
#include "scene/sphere.h"
#include <chrono>
#include <memory>

class Application
{
public:
    Application();
    ~Application();

    void run();
    void update();
    void renderDepthPass();
    void renderMainPass();
    void renderImGui();

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
    std::chrono::high_resolution_clock::time_point m_fpsWindowStart{};

    // GL-dependent resources are deferred
    std::unique_ptr<RobotArm> m_robotArm;
    std::unique_ptr<Model> m_asset1;
    std::unique_ptr<Skybox> m_skybox;
    std::unique_ptr<ShadowMap> m_sunShadow;
    std::unique_ptr<ShadowMap> m_spotShadow;
    std::unique_ptr<Shader> m_modelShader;
    std::unique_ptr<Shader> m_skyboxShader;
    std::unique_ptr<Shader> m_depthShader;
};

#endif // APPLICATION_H