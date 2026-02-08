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
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

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
    m_window.setCamera(&m_camera);

    m_icosahedron = std::make_unique<Model>("../assets/icosahedron-low.obj");
    m_planet = std::make_unique<Model>("../assets/planet/planet.obj");
    m_skybox = std::make_unique<Skybox>();

    m_sunShadow = std::make_unique<ShadowMap>();
    m_spotShadow = std::make_unique<ShadowMap>();

    m_modelShader = std::make_unique<Shader>("shaders/model.vs", "shaders/basic_phong.fs");
    m_skyboxShader = std::make_unique<Shader>("shaders/skybox.vs", "shaders/skybox.fs");
    m_depthShader = std::make_unique<Shader>("shaders/shadowMapping.vs", "shaders/shadowMapping.fs");
    m_instancedModelShader = std::make_unique<Shader>("shaders/model_instanced.vs", "shaders/basic_phong.fs");
    m_instancedDepthShader = std::make_unique<Shader>("shaders/shadowMapping_instanced.vs", "shaders/shadowMapping.fs");

    m_icosahedron->setupInstanceBuffers(kSliderMax);
    m_asteroidTransforms.reserve(kSliderMax);
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
        auto frameStart = std::chrono::system_clock::now();

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

        // update window and scene objects
        update();

        // render
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // renderDepthPass();
        {
            auto drawStart = std::chrono::system_clock::now();
            renderMainPass();
            auto drawEnd = std::chrono::system_clock::now();
            auto drawElapsed = std::chrono::duration_cast<std::chrono::microseconds>(drawEnd - drawStart);
            m_stats.meshDrawTime = drawElapsed.count() / 1000.f;
        }
        renderImGui();

        double beforeSwap = glfwGetTime();
        glfwSwapBuffers(m_window.getGlfwWindow());
        double afterSwap = glfwGetTime();

        m_swapTime = (afterSwap - beforeSwap) * 1000.0;

        // glfwSwapBuffers(m_window.getGlfwWindow());
        glfwPollEvents();

        auto frameEnd = std::chrono::system_clock::now();
        auto frameElapsed = std::chrono::duration_cast<std::chrono::microseconds>(frameEnd - frameStart);
        m_stats.frameTime = frameElapsed.count() / 1000.f;
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
    // m_spotlight.update();
}

void Application::renderDepthPass()
{
    // 1. sun depth pass
    m_sunShadow->bind();
    m_icosahedron->drawShadowMap(*m_depthShader, m_sunLight.getLightSpaceMatrix(), m_icosahedron->getModelMatrix());
    m_sunShadow->unbind();

    // // 2. spotlight depth pass
    // m_spotShadow->bind();
    // m_asset1->drawShadowMap(*m_depthShader, m_spotlight.getSpotLightSpaceMatrix(), m_asset1->getModelMatrix());
    // m_spotShadow->unbind();
}

void Application::renderMainPass()
{
    m_stats.drawcallCount = 0;
    m_stats.triangleCount = 0;

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

    std::mt19937 rng(42);
    std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
    std::uniform_real_distribution<float> radiusDist(0.0f, 1.0f);
    std::uniform_real_distribution<float> scaleDist(_minScale, _maxScale);
    std::uniform_real_distribution<float> rotDist(0.0f, glm::two_pi<float>());

    // instanced path
    if (useInstancing && numAsteroids > 0)
    {
        m_asteroidTransforms.resize(numAsteroids);
        for (int i = 0; i < numAsteroids; ++i)
        {
            float u = angleDist(rng) + _asteroidTime;
            float v = angleDist(rng);
            float randomVariation = _minorRadius * radiusDist(rng);
            float x = (_majorRadius + randomVariation * std::cos(v)) * std::cos(u);
            float z = (_majorRadius + randomVariation * std::cos(v)) * std::sin(u);
            float y = randomVariation * std::sin(v) * _verticalScale;

            float scale = scaleDist(rng);

            float rotX = rotDist(rng) + _asteroidTime * kRotationSpeed;
            float rotY = rotDist(rng) + _asteroidTime * kRotationSpeed;
            float rotZ = rotDist(rng) + _asteroidTime * kRotationSpeed;

            glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
            glm::mat4 R = glm::rotate(glm::mat4(1.0f), rotX, glm::vec3(1, 0, 0));
            R = glm::rotate(R, rotY, glm::vec3(0, 1, 0));
            R = glm::rotate(R, rotZ, glm::vec3(0, 0, 1));
            glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
            m_asteroidTransforms[i] = T * R * S;
        }

        m_icosahedron->updateInstanceData(m_asteroidTransforms.data(), numAsteroids);

        m_icosahedron->configureShader(*m_instancedModelShader, m_camera, m_sunLight, m_spotlight, m_spotlightGain);
        m_instancedModelShader->use();
        m_instancedModelShader->setMat4("projection", projection);
        m_instancedModelShader->setMat4("view", view);
        m_icosahedron->drawInstanced(*m_instancedModelShader, numAsteroids);
        m_stats.drawcallCount++;
        m_stats.triangleCount += (m_icosahedron->getTotalIndexCount() / 3) * numAsteroids;
    }
    // non-instanced path
    else
    {
        m_icosahedron->configureShader(*m_modelShader, m_camera, m_sunLight, m_spotlight, m_spotlightGain);
        for (int i = 0; i < numAsteroids; ++i)
        {
            float u = angleDist(rng) + _asteroidTime;
            float v = angleDist(rng);
            float randomVariation = _minorRadius * radiusDist(rng);

            // polar coordinates to XZ
            float x = (_majorRadius + randomVariation * std::cos(v)) * std::cos(u);
            float z = (_majorRadius + randomVariation * std::cos(v)) * std::sin(u);
            float y = randomVariation * std::sin(v) * _verticalScale;

            float scale = scaleDist(rng);

            float rotX = rotDist(rng) + _asteroidTime * kRotationSpeed;
            float rotY = rotDist(rng) + _asteroidTime * kRotationSpeed;
            float rotZ = rotDist(rng) + _asteroidTime * kRotationSpeed;

            glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
            glm::mat4 R = glm::rotate(glm::mat4(1.0f), rotX, glm::vec3(1, 0, 0));
            R = glm::rotate(R, rotY, glm::vec3(0, 1, 0));
            R = glm::rotate(R, rotZ, glm::vec3(0, 0, 1));
            glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(scale));
            m_modelShader->setMat4("model", T * R * S);
            m_icosahedron->draw(*m_modelShader, projection, view, m_camera, m_sunLight.getSunPosition(),
                                glm::vec3(0.0f));
            m_stats.drawcallCount++;
            m_stats.triangleCount += m_icosahedron->getTotalIndexCount() / 3;
        }
    }
    // wrap around every 2 pi because of floating point precision
    // asteroid belt rotates counter-clockwise viewed from north pole
    _asteroidTime -= 0.05f * deltaTime;
    if (_asteroidTime < -glm::two_pi<float>())
    {
        _asteroidTime += glm::two_pi<float>();
    }

    m_planet->configureShader(*m_modelShader, m_camera, m_sunLight, m_spotlight, m_spotlightGain);
    glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
    m_modelShader->setMat4("model", model);
    m_planet->draw(*m_modelShader, projection, view, m_camera, m_sunLight.getSunPosition(), glm::vec3(0.0f));
    m_stats.drawcallCount++;
    m_stats.triangleCount += m_planet->getTotalIndexCount() / 3;
}

void Application::renderImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(15, 18), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(261, 190), ImGuiCond_FirstUseEver);
    ImGui::Begin("Stats");

    if (ImGui::BeginTable("stats_table", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 130.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("frametime");
        ImGui::TableNextColumn();
        ImGui::Text("%0.3f ms", m_stats.frameTime);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("drawtime");
        ImGui::TableNextColumn();
        ImGui::Text("%0.3f ms", m_stats.meshDrawTime);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("triangles");
        ImGui::TableNextColumn();
        ImGui::Text("%i", m_stats.triangleCount);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("draws");
        ImGui::TableNextColumn();
        ImGui::Text("%i", m_stats.drawcallCount);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("FPS");
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", ImGui::GetIO().Framerate);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("avg FPS (5 sec)");
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", m_avgFps);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Separator();
        ImGui::TableSetColumnIndex(1);
        ImGui::Separator();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("num of asteroids");
        ImGui::TableNextColumn();
        ImGui::SetNextItemWidth(-FLT_MIN);
        ImGui::SliderScalar("##num_asteroids", ImGuiDataType_S32, &numAsteroids, &kSliderMin, &kSliderMax, "%u");

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("instancing (I)");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##instancing", &useInstancing);

        ImGui::EndTable();
    }

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(289, 19), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(411, 150), ImGuiCond_FirstUseEver);
    ImGui::Begin("Controls");
    if (ImGui::BeginTable("controls_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableHeadersRow();
        const std::array<std::pair<const char*, const char*>, 4> controls = {{
            {"WASD", "Move camera"},
            {"J / K", "Increase / Decrease num of asteroids"},
            {"Left Shift", "Speed boost while moving"},
            {"I", "Toggle instancing"},
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
