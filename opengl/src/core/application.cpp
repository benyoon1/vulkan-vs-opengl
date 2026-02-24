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
#include <algorithm>
#include <array>
#include <chrono>
#include <random>
#ifdef __APPLE__
#include <sys/sysctl.h>
#endif

#include "core/application.h"
#include "core/utils.h"
// clang-format on

Application::Application(int initialScene) : m_window(), m_camera(), m_sunLight(), m_spotlight()
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

    m_skybox = std::make_unique<Skybox>();

    m_sunShadow = std::make_unique<ShadowMap>();
    m_spotShadow = std::make_unique<ShadowMap>();

    m_modelShader = std::make_unique<Shader>("shaders/model.vs", "shaders/basic_phong.fs");
    m_skyboxShader = std::make_unique<Shader>("shaders/skybox.vs", "shaders/skybox.fs");
    m_skyboxCubemapShader = std::make_unique<Shader>("shaders/skybox.vs", "shaders/skybox_cubemap.fs");
    m_depthShader = std::make_unique<Shader>("shaders/shadowMapping.vs", "shaders/shadowMapping.fs");
    m_instancedModelShader = std::make_unique<Shader>("shaders/model_instanced.vs", "shaders/basic_phong.fs");
    m_instancedDepthShader = std::make_unique<Shader>("shaders/shadowMapping_instanced.vs", "shaders/shadowMapping.fs");

    glGenQueries(1, &m_gpuTimerQuery);

    m_asteroidTransforms.reserve(kSliderMax);

    // clang-format off
    m_sceneRegistry.push_back({
        .name       = "synthetic scene (planet & asteroids)",
        .assetPath  = "../assets/icosahedron-low.obj",
        .type       = SceneType::PlanetAndAsteroids,
        .scale      = 1.0f,
        .cameraStartPos = glm::vec3(5.0f, 0.0f, 23.0f),
        .sunStartPos    = glm::vec3(0.0f, 0.0f, 100.0f),
        .skyboxDir      = "",
    });
    m_sceneRegistry.push_back({
        .name       = "amazon bistro",
        .assetPath  = "../assets/bistro/bistro.obj",
        .type       = SceneType::AmazonBistro,
        .scale      = 0.5f,
        .cameraStartPos = glm::vec3(-5.0f, 3.0f, 0.0f),
        .sunStartPos    = glm::vec3(0.0f, 50.0f, 20.0f),
        .skyboxDir      = "../assets/skybox",
    });
    // clang-format on

    loadScene(initialScene);
    initDeviceInfo();
}

void Application::loadScene(int index)
{
    if (index < 0 || index >= static_cast<int>(m_sceneRegistry.size()))
        return;

    m_currentSceneIndex = index;
    const auto& entry = m_sceneRegistry[index];

    m_camera.setPosition(entry.cameraStartPos);
    m_sunLight.setSunPosition(entry.sunStartPos);

    if (entry.skyboxDir.empty())
    {
        m_skybox->loadCubemap({"", "", "", "", "", ""});
    }
    else
    {
        std::string d = Utils::getPath(entry.skyboxDir);
        m_skybox->loadCubemap(
            {d + "/px.png", d + "/nx.png", d + "/py.png", d + "/ny.png", d + "/pz.png", d + "/nz.png"});
    }

    // reset scene-specific resources
    m_icosahedron.reset();
    m_planet.reset();
    m_bistro.reset();

    switch (entry.type)
    {
        case SceneType::PlanetAndAsteroids:
            m_icosahedron = std::make_unique<Model>("../assets/icosahedron-low.obj");
            m_planet = std::make_unique<Model>("../assets/planet/planet.obj");
            m_icosahedron->setupInstanceBuffers(kSliderMax);
            break;
        case SceneType::AmazonBistro:
            m_bistro = std::make_unique<Model>(entry.assetPath);
            break;
    }
}

Application::~Application()
{
    glDeleteQueries(1, &m_gpuTimerQuery);
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

        if (useShadowMap)
        {
            renderDepthPass();
        }
        // measure main pass only for CPU/GPU time
        {
            auto drawStart = std::chrono::system_clock::now();
            glBeginQuery(GL_TIME_ELAPSED, m_gpuTimerQuery);
            renderMainPass();
            glEndQuery(GL_TIME_ELAPSED);
            auto drawEnd = std::chrono::system_clock::now();
            auto drawElapsed = std::chrono::duration_cast<std::chrono::microseconds>(drawEnd - drawStart);
            m_stats.cpuDrawTime = drawElapsed.count() / 1000.f;

            GLuint64 gpuTimeNs = 0;
            glGetQueryObjectui64v(m_gpuTimerQuery, GL_QUERY_RESULT, &gpuTimeNs);
            m_stats.gpuDrawTime = static_cast<float>(gpuTimeNs) / 1e6f;
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

        // record frame time for graph
        m_stats.frameTimeGraph[m_stats.frameTimeGraphIndex % EngineStats::kGraphSize] = m_stats.frameTime;
        m_stats.frameTimeGraphIndex++;

        // record frame time for percentile FPS
        m_stats.frameTimeHistory[m_stats.frameTimeHistoryIndex % EngineStats::kPercentileWindow] = m_stats.frameTime;
        m_stats.frameTimeHistoryIndex++;
        if (m_stats.frameTimeHistoryIndex >= EngineStats::kPercentileWindow)
            m_stats.frameTimeHistoryFilled = true;

        // compute 1% low and 0.1% low FPS every kSampleInterval frames
        if (m_stats.frameTimeHistoryIndex % EngineStats::kSampleInterval == 0)
        {
            int count = m_stats.frameTimeHistoryFilled ? EngineStats::kPercentileWindow : m_stats.frameTimeHistoryIndex;
            if (count > 0)
            {
                std::array<float, EngineStats::kPercentileWindow> sorted;
                std::copy_n(m_stats.frameTimeHistory.begin(), count, sorted.begin());
                std::sort(sorted.begin(), sorted.begin() + count, std::greater<float>());

                // 1% low: average of the worst 1% frame times -> convert to FPS
                int n1 = std::max(1, count / 100);
                float sum1 = 0.f;
                for (int i = 0; i < n1; i++)
                    sum1 += sorted[i];
                m_stats.fps1Low = 1000.f / (sum1 / static_cast<float>(n1));

                // 0.1% low: average of the worst 0.1% frame times
                int n01 = std::max(1, count / 1000);
                float sum01 = 0.f;
                for (int i = 0; i < n01; i++)
                    sum01 += sorted[i];
                m_stats.fps01Low = 1000.f / (sum01 / static_cast<float>(n01));
            }
        }
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
    const auto& sceneEntry = m_sceneRegistry[m_currentSceneIndex];

    m_sunShadow->bind();

    switch (sceneEntry.type)
    {
        case SceneType::PlanetAndAsteroids:
        {
            if (useInstancing && numAsteroids > 0)
            {
                m_icosahedron->drawShadowMapInstanced(*m_instancedDepthShader, m_sunLight.getLightSpaceMatrix(),
                                                      numAsteroids);
            }
            else
            {
                std::mt19937 rng(42);
                std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
                std::uniform_real_distribution<float> radiusDist(0.0f, 1.0f);
                std::uniform_real_distribution<float> scaleDist(_minScale, _maxScale);
                std::uniform_real_distribution<float> rotDist(0.0f, glm::two_pi<float>());

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

                    m_icosahedron->drawShadowMap(*m_depthShader, m_sunLight.getLightSpaceMatrix(), T * R * S);
                }
            }
            glm::mat4 planetModel = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
            m_planet->drawShadowMap(*m_depthShader, m_sunLight.getLightSpaceMatrix(), planetModel);
            break;
        }
        case SceneType::AmazonBistro:
        {
            glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(sceneEntry.scale));
            m_bistro->drawShadowMap(*m_depthShader, m_sunLight.getLightSpaceMatrix(), model);
            break;
        }
    }

    m_sunShadow->unbind();
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

    m_sunShadow->bindTexture(GL_TEXTURE0 + ShadowMap::kSunShadowTextureNum);

    const auto& sceneEntry = m_sceneRegistry[m_currentSceneIndex];

    switch (sceneEntry.type)
    {
        case SceneType::PlanetAndAsteroids:
        {
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

                m_icosahedron->configureShader(*m_instancedModelShader, m_camera, m_sunLight, m_spotlight,
                                               m_spotlightGain);
                m_instancedModelShader->use();
                m_instancedModelShader->setMat4("projection", projection);
                m_instancedModelShader->setMat4("view", view);
                m_instancedModelShader->setInt("useShadowMap", useShadowMap ? 1 : 0);
                m_icosahedron->drawInstanced(*m_instancedModelShader, numAsteroids);
                m_stats.drawcallCount++;
                m_stats.triangleCount += (m_icosahedron->getTotalIndexCount() / 3) * numAsteroids;
            }
            // non-instanced path
            else
            {
                m_icosahedron->configureShader(*m_modelShader, m_camera, m_sunLight, m_spotlight, m_spotlightGain);
                m_modelShader->setInt("useShadowMap", useShadowMap ? 1 : 0);
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
            // asteroid belt rotates counter-clockwise when viewed from north pole
            _asteroidTime -= 0.05f * deltaTime;
            if (_asteroidTime < -glm::two_pi<float>())
            {
                _asteroidTime += glm::two_pi<float>();
            }

            m_planet->configureShader(*m_modelShader, m_camera, m_sunLight, m_spotlight, m_spotlightGain);
            m_modelShader->setInt("useShadowMap", useShadowMap ? 1 : 0);
            glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
            m_modelShader->setMat4("model", model);
            m_planet->draw(*m_modelShader, projection, view, m_camera, m_sunLight.getSunPosition(), glm::vec3(0.0f));
            m_stats.drawcallCount++;
            m_stats.triangleCount += m_planet->getTotalIndexCount() / 3;
            break;
        }
        case SceneType::AmazonBistro:
        {
            m_bistro->configureShader(*m_modelShader, m_camera, m_sunLight, m_spotlight, m_spotlightGain);
            m_modelShader->setInt("useShadowMap", useShadowMap ? 1 : 0);
            glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(sceneEntry.scale));
            m_modelShader->setMat4("model", model);
            m_bistro->draw(*m_modelShader, projection, view, m_camera, m_sunLight.getSunPosition(), glm::vec3(0.0f));
            m_stats.drawcallCount++;
            m_stats.triangleCount += m_bistro->getTotalIndexCount() / 3;

            m_skybox->draw(*m_skyboxCubemapShader, projection, m_camera, m_sunLight.getSunDirection(),
                           glm::vec2(width, height));
            break;
        }
    }
}

void Application::renderImGui()
{
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    ImGui::SetNextWindowPos(ImVec2(15, 18), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(285, 420), ImGuiCond_FirstUseEver);
    ImGui::Begin("Stats");

    // scene selector
    ImGui::TextUnformatted("current scene:");
    const char* currentSceneName = m_sceneRegistry[m_currentSceneIndex].name.c_str();
    if (ImGui::BeginCombo("##scene_select", currentSceneName))
    {
        for (int i = 0; i < static_cast<int>(m_sceneRegistry.size()); ++i)
        {
            bool isSelected = (m_currentSceneIndex == i);
            if (ImGui::Selectable(m_sceneRegistry[i].name.c_str(), isSelected))
            {
                loadScene(i);
            }
            if (isSelected)
                ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    ImGui::Separator();

    // device info
    ImGui::Text("GPU: %s", m_stats.gpuName.c_str());
    ImGui::Text("Mac: %s", m_stats.macModel.c_str());
    ImGui::Text("OpenGL: %s", m_stats.glVersion.c_str());
    {
        int width, height;
        glfwGetFramebufferSize(m_window.getGlfwWindow(), &width, &height);
        ImGui::Text("resolution: %dx%d", width, height);
    }
    ImGui::Separator();

    if (ImGui::BeginTable("stats_table", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableSetupColumn("Label", ImGuiTableColumnFlags_WidthFixed, 150.0f);
        ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("frametime");
        ImGui::TableNextColumn();
        ImGui::Text("%0.3f ms", m_stats.frameTime);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("CPU draw");
        ImGui::TableNextColumn();
        ImGui::Text("%0.3f ms", m_stats.cpuDrawTime);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("GPU draw");
        ImGui::TableNextColumn();
        ImGui::Text("%0.3f ms", m_stats.gpuDrawTime);

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
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("1% low FPS");
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", m_stats.fps1Low);

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("0.1% low FPS");
        ImGui::TableNextColumn();
        ImGui::Text("%.1f", m_stats.fps01Low);

        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Separator();
        ImGui::TableSetColumnIndex(1);
        ImGui::Separator();

        if (m_sceneRegistry[m_currentSceneIndex].type == SceneType::PlanetAndAsteroids)
        {
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
        }

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextUnformatted("shadow map");
        ImGui::TableNextColumn();
        ImGui::Checkbox("##shadowmap", &useShadowMap);

        ImGui::EndTable();
    }

    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(289, 19), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(411, 120), ImGuiCond_FirstUseEver);
    ImGui::Begin("Controls");
    if (ImGui::BeginTable("controls_table", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
    {
        ImGui::TableSetupColumn("Key", ImGuiTableColumnFlags_WidthStretch, 0.3f);
        ImGui::TableSetupColumn("Description", ImGuiTableColumnFlags_WidthStretch, 0.7f);
        ImGui::TableHeadersRow();
        const std::array<std::pair<const char*, const char*>, 3> controls = {{
            {"WASD", "Move camera"},
            {"Mouse drag", "Pan camera"},
            {"Left Shift", "Speed boost while moving"},
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

    // frame time graph window
    {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        float graphHeight{150.f};
        float padding{10.f};
        ImGui::SetNextWindowPos(
            ImVec2(viewport->WorkPos.x + padding, viewport->WorkPos.y + viewport->WorkSize.y - graphHeight - padding),
            ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(viewport->WorkSize.x - padding * 2.f, graphHeight), ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(0.8f);
        ImGui::Begin("##frametime_window", nullptr,
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav);

        constexpr float graphMax{32.f};
        constexpr float ticks[] = {0.f, 8.f, 16.f, 24.f, 32.f};
        constexpr int numTicks = sizeof(ticks) / sizeof(ticks[0]);

        float tickLabelWidth = ImGui::CalcTextSize("32ms").x + 8.f;
        ImVec2 contentMin = ImGui::GetCursorScreenPos();
        float availWidth = ImGui::GetContentRegionAvail().x;
        float availHeight = ImGui::GetContentRegionAvail().y;

        // graph area (right of tick labels)
        float graphX = contentMin.x + tickLabelWidth;
        float graphW = availWidth - tickLabelWidth;
        float graphY = contentMin.y;
        float graphH = availHeight;

        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + tickLabelWidth);
        int offset = m_stats.frameTimeGraphIndex % EngineStats::kGraphSize;
        ImGui::PlotLines("##frametime_graph", m_stats.frameTimeGraph.data(), EngineStats::kGraphSize, offset,
                         "frame time (ms)", 0.f, graphMax, ImVec2(graphW, graphH));

        // draw y-axis tick labels and horizontal guide lines
        {
            ImDrawList* drawList = ImGui::GetWindowDrawList();
            ImU32 textColor = ImGui::GetColorU32(ImGuiCol_TextDisabled);
            ImU32 lineColor = ImGui::GetColorU32(ImGuiCol_TextDisabled, 0.3f);

            for (int i = 0; i < numTicks; i++)
            {
                float t = ticks[i] / graphMax;
                float y = graphY + graphH - t * graphH;

                char tickLabel[16];
                snprintf(tickLabel, sizeof(tickLabel), "%.0fms", ticks[i]);
                ImVec2 labelSize = ImGui::CalcTextSize(tickLabel);
                drawList->AddText(ImVec2(graphX - labelSize.x - 4.f, y - labelSize.y / 2.f), textColor, tickLabel);

                if (ticks[i] > 0.f)
                {
                    drawList->AddLine(ImVec2(graphX, y), ImVec2(graphX + graphW, y), lineColor);
                }
            }
        }

        ImGui::End();
    }

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

void Application::initDeviceInfo()
{
    m_stats.gpuName = reinterpret_cast<const char*>(glGetString(GL_RENDERER));
    m_stats.glVersion = reinterpret_cast<const char*>(glGetString(GL_VERSION));

#ifdef __APPLE__
    char model[256]{};
    size_t modelLen = sizeof(model);
    if (sysctlbyname("hw.model", model, &modelLen, nullptr, 0) == 0)
        m_stats.macModel = model;
    else
        m_stats.macModel = "Unknown Mac";
#else
    m_stats.macModel = "Linux/Windows PC";
#endif
}
