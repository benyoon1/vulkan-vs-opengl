// clang-format off
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stb_image.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <stdexcept>
#include <array>
#include <chrono>
#include <random>

#include "core/application.h"
// clang-format on

Application::Application() : m_window(), m_camera(), m_sunLight(), m_spotlight()
{
    // glad: load all OpenGL function pointers
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        throw std::runtime_error("Failed to initialize GLAD");
    }
    glEnable(GL_DEPTH_TEST);

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(m_window.getGlfwWindow(), true);
    ImGui_ImplOpenGL3_Init("#version 410"); // TODO: match GL version with window

    // construct GL-dependent resources AFTER GLAD
    m_robotArm = std::make_unique<RobotArm>("../assets/robot_arm/wrist.obj", "../assets/robot_arm/lower_arm.obj",
                                            "../assets/robot_arm/upper_arm.obj");
    m_window.setCamera(&m_camera);
    m_window.setRobotArm(m_robotArm.get());

    m_asset1 = std::make_unique<Model>("../assets/icosahedron-low.obj");
    m_skybox = std::make_unique<Skybox>();

    m_sunShadow = std::make_unique<ShadowMap>();
    m_spotShadow = std::make_unique<ShadowMap>();

    m_modelShader = std::make_unique<Shader>("shaders/model.vs", "shaders/basic_phong.fs");
    m_skyboxShader = std::make_unique<Shader>("shaders/skybox.vs", "shaders/skybox.fs");
    m_depthShader = std::make_unique<Shader>("shaders/shadowMapping.vs", "shaders/shadowMapping.fs");
}

Application::~Application()
{
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void Application::run()
{
    while (!glfwWindowShouldClose(m_window.getGlfwWindow()))
    {
        auto currFrameTime = std::chrono::high_resolution_clock::now();
        if (m_fpsFrameCount == 0 && m_fpsWindowStart.time_since_epoch().count() == 0)
        {
            m_fpsWindowStart = currFrameTime;
        }
        m_fpsFrameCount++;

        float elapsedSec = std::chrono::duration<float>(currFrameTime - m_fpsWindowStart).count();
        if (elapsedSec >= 5.0f)
        {
            m_avgFps = static_cast<float>(m_fpsFrameCount) / elapsedSec;
            m_fpsFrameCount = 0;
            m_fpsWindowStart = currFrameTime;
        }

        // double frameStart = glfwGetTime();

        // update window and scene objects
        update();

        // render
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // renderDepthPass();
        renderMainPass();
        renderImGui();

        double beforeSwap = glfwGetTime();
        glfwSwapBuffers(m_window.getGlfwWindow());
        double afterSwap = glfwGetTime();

        // Print occasionally to see where time is spent
        // static int frameCount = 0;
        // if (++frameCount % 300 == 0)
        // {
        //     printf("Render: %.2fms, Swap: %.2fms\n", (beforeSwap - frameStart) * 1000.0,
        //            (afterSwap - beforeSwap) * 1000.0);
        // }
        m_swapTime = (afterSwap - beforeSwap) * 1000.0;

        // glfwSwapBuffers(m_window.getGlfwWindow());
        glfwPollEvents();
    }
}

void Application::updateFrame()
{
    m_currentFrame = static_cast<float>(glfwGetTime());
    deltaTime = m_currentFrame - m_lastFrame;
    m_lastFrame = m_currentFrame;
}

void Application::update()
{
    // reset values every frame
    m_sunSpeed = 0.1f;
    m_spotlightGain = 1.0f;

    updateFrame();
    m_window.processInput(this);

    // scene update
    m_sunLight.update(m_sunSpeed);
    m_robotArm->updateWristPose(m_camera);
    m_spotlight.update(*m_robotArm);
}

void Application::renderDepthPass()
{
    // 1. sun depth pass
    m_sunShadow->bind();
    m_asset1->drawShadowMap(*m_depthShader, m_sunLight.getLightSpaceMatrix(), m_asset1->getModelMatrix());
    m_robotArm->drawShadowMap(*m_depthShader, m_sunLight.getLightSpaceMatrix());
    m_sunShadow->unbind();

    // // 2. spotlight depth pass
    // m_spotShadow->bind();
    // m_asset1->drawShadowMap(*m_depthShader, m_spotlight.getSpotLightSpaceMatrix(), m_asset1->getModelMatrix());
    // m_spotShadow->unbind();
}

void Application::renderMainPass()
{
    int width, height;
    glfwGetFramebufferSize(m_window.getGlfwWindow(), &width, &height); // high DPI bugfix
    glViewport(0, 0, width, height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float aspect = static_cast<float>(width) / static_cast<float>(height > 0 ? height : 1);
    glm::mat4 projection = glm::perspective(glm::radians(m_camera.getZoom()), aspect, 0.1f, 5000.0f);
    glm::mat4 view = m_camera.getViewMatrix();

    // bind shadow maps to texture units
    m_sunShadow->bindTexture(GL_TEXTURE0 + ShadowMap::kSunShadowTextureNum);
    m_spotShadow->bindTexture(GL_TEXTURE0 + ShadowMap::kSpotShadowTextureNum);

    m_asset1->configureShader(*m_modelShader, m_camera, m_sunLight, m_spotlight, *m_robotArm, m_spotlightGain);

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
    std::uniform_real_distribution<float> radiusDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> scaleDist(_minScale, _maxScale);
    std::uniform_real_distribution<float> rotDist(0.0f, glm::two_pi<float>());

    for (int i = 0; i < numAsteroids; ++i)
    {
        float u = angleDist(rng) + _asteroidTime; // angle around the major circle [0, 2π]
        float v = angleDist(rng);                 // angle around the minor circle [0, 2π]

        // add random variation in [0,1] range
        float randomVariation = _minorRadius * radiusDist(rng);

        // polar coordinates to XZ
        float x = (_majorRadius + randomVariation * std::cos(v)) * std::cos(u);
        float z = (_majorRadius + randomVariation * std::cos(v)) * std::sin(u);
        float y = randomVariation * std::sin(v) * _verticalScale;

        float scale = scaleDist(rng);

        float rotX = rotDist(rng) + _asteroidTime;
        float rotY = rotDist(rng) + _asteroidTime;
        float rotZ = rotDist(rng) + _asteroidTime;

        glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
        glm::mat4 R = glm::rotate(glm::mat4(1.0f), rotX, glm::vec3(1, 0, 0));
        R = glm::rotate(R, rotY, glm::vec3(0, 1, 0));
        R = glm::rotate(R, rotZ, glm::vec3(0, 0, 1));
        glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
        m_modelShader->setMat4("model", T * R * S);
        m_asset1->draw(*m_modelShader, projection, view, m_camera, m_sunLight.getSunPosition(), glm::vec3(0.0f));
    }
    // wrap around every 2 pi because of floating point precision
    // asteroid belt rotates counter-clockwise viewed from north pole
    _asteroidTime -= 0.05f * deltaTime;
    if (_asteroidTime < -glm::two_pi<float>())
    {
        _asteroidTime += glm::two_pi<float>();
    }
}

void Application::renderImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(15, 18), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(261, 190), ImGuiCond_FirstUseEver);
    ImGui::Begin("Stats");

    // ImGui::Text("frametime %f ms", stats.frametime);
    // ImGui::Text("drawtime %f ms", stats.mesh_draw_time);
    // ImGui::Text("triangles %i", stats.triangle_count);
    // ImGui::Text("draws %i", stats.drawcall_count);
    ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
    ImGui::Text("avg FPS (5s): %.1f", m_avgFps);
    // ImGui::Text("swap time: %.2f ms", m_swapTime);
    ImGui::Separator();
    ImGui::SliderScalar("num of asteroids", ImGuiDataType_S32, &numAsteroids, &kSliderMin, &kSliderMax, "%u");
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(289, 19), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(411, 190), ImGuiCond_FirstUseEver);
    ImGui::Begin("Controls");
    if (ImGui::BeginTable("controls_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableHeadersRow();
        const std::array<std::pair<const char*, const char*>, 8> controls = {{
            {"WASD", "Move camera"},
            {"Mouse drag", "Pan camera"},
            {"J / K", "Increase / Decrease num of asteroids"},
            {"Left Shift", "Run / speed boost while moving"},
        }};
        for (const auto& [key, desc] : controls)
        {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(key);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(desc);
        }
        ImGui::EndTable();
    }
    ImGui::End();

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}
