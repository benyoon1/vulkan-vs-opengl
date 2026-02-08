#include "scene.h"

#include "camera.h"
#include "vk_context.h"
#include "vk_material.h"
#include "vk_resources.h"

#include "SDL_timer.h"
#include <SDL_keyboard.h>
#include <SDL_scancode.h>
#include <glm/gtx/transform.hpp>

#include <random>

void Scene::initRenderables(VulkanContext& ctx, ResourceManager& resources, GLTFMetallic_Roughness& material)
{
    const std::string icosahedron = asset_path("icosahedron-low.obj");
    auto asset1 = loadAssimpAssets(ctx, resources, material, icosahedron);
    if (asset1.has_value())
    {
        loadedAssets["icosahedron"] = *asset1;
        auto& gltf = *asset1;
        for (auto& [name, mesh] : gltf->meshes)
        {
            if (mesh && !mesh->surfaces.empty())
            {
                _icosahedronMesh = mesh;
                break;
            }
        }
    }
    else
    {
        fmt::print("Warning: failed to load icosahedron-low.obj from '{}'.\n", icosahedron);
    }

    const std::string planet = asset_path("planet/planet.obj");
    auto asset2 = loadAssimpAssets(ctx, resources, material, planet);
    if (asset2.has_value())
    {
        loadedAssets["planet"] = *asset2;
    }
    else
    {
        fmt::print("Warning: failed to load planet/planet.obj from '{}'.\n", planet);
    }
}

void Scene::updateFrame()
{
    _currentFrame = static_cast<float>(SDL_GetTicks64()) / 1000.0f;
    _deltaTime = _currentFrame - _lastFrame;
    _lastFrame = _currentFrame;
}

void Scene::update(VkExtent2D& windowExtent, DrawContext& drawCommands, Camera& mainCamera, DirectionalLight& sunLight)
{
    updateFrame();
    mainCamera.processInput(_deltaTime);
    sunLight.update();

    glm::mat4 view = mainCamera.getViewMatrix();

    glm::mat4 projection = glm::perspective(glm::radians(mainCamera.getFOV()),
                                            (float)windowExtent.width / (float)windowExtent.height, 10000.f, 0.1f);

    // invert the Y direction on projection matrix so that we are more similar
    // to opengl and gltf axis
    projection[1][1] *= -1;

    sceneData.sunlightPosition = glm::vec4(sunLight.getSunPosition(), 1.0f);
    sceneData.cameraPosition = glm::vec4(mainCamera.getPosition(), 1.0f);
    sceneData.sunlightColor = glm::vec4(1.0f);

    sceneData.sunlightViewProj = sunLight.getLightSpaceMatrix();

    // spotlight
    // sceneData.spotColor = glm::vec4(SpotlightConstants::kSpotColor, 1.0f);
    // float innerCutoff = glm::cos(glm::radians(SpotlightConstants::kInnerCutDeg));
    // float outerCutoff = glm::cos(glm::radians(SpotlightConstants::kOuterCutDeg));
    // float intensity = SpotlightConstants::kIntensity * spotlight.spotGain;
    // sceneData.spotCutoffAndIntensity = glm::vec4(innerCutoff, outerCutoff, intensity, 0.0f);

    drawCommands.viewProj = projection * view;

    auto it = loadedAssets.find("icosahedron");
    if (it != loadedAssets.end() && it->second)
    {
        std::mt19937 rng(42);
        std::uniform_real_distribution<float> angleDist(0.0f, glm::two_pi<float>());
        std::uniform_real_distribution<float> radiusDist(0.0f, 1.0f);
        std::uniform_real_distribution<float> scaleDist(minScale, maxScale);
        std::uniform_real_distribution<float> rotDist(0.0f, glm::two_pi<float>());

        if (useInstancing && _icosahedronMesh && !_icosahedronMesh->surfaces.empty())
        {
            asteroidTransforms.clear();
            asteroidTransforms.reserve(numAsteroids);

            for (int i = 0; i < numAsteroids; ++i)
            {
                float u = angleDist(rng) + _asteroidTime;
                float v = angleDist(rng);

                float randomVariation = minorRadius * radiusDist(rng);

                float x = (majorRadius + randomVariation * std::cos(v)) * std::cos(u);
                float z = (majorRadius + randomVariation * std::cos(v)) * std::sin(u);
                float y = randomVariation * std::sin(v) * verticalScale;

                float scale = scaleDist(rng);

                float rotX = rotDist(rng) + _asteroidTime * kRotationSpeed;
                float rotY = rotDist(rng) + _asteroidTime * kRotationSpeed;
                float rotZ = rotDist(rng) + _asteroidTime * kRotationSpeed;

                glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
                glm::mat4 R = glm::rotate(glm::mat4(1.0f), rotX, glm::vec3(1, 0, 0));
                R = glm::rotate(R, rotY, glm::vec3(0, 1, 0));
                R = glm::rotate(R, rotZ, glm::vec3(0, 0, 1));
                glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(scale));

                asteroidTransforms.push_back(T * R * S);
            }

            auto& surface = _icosahedronMesh->surfaces[0];
            instancedMeshInfo.indexCount = surface.count;
            instancedMeshInfo.firstIndex = surface.startIndex;
            instancedMeshInfo.indexBuffer = _icosahedronMesh->meshBuffers.indexBuffer.buffer;
            instancedMeshInfo.material = &surface.material->data;
            instancedMeshInfo.vertexBufferAddress = _icosahedronMesh->meshBuffers.vertexBufferAddress;
        }
        else
        {
            asteroidTransforms.clear();

            for (int i = 0; i < numAsteroids; ++i)
            {
                float u = angleDist(rng) + _asteroidTime;
                float v = angleDist(rng);

                float randomVariation = minorRadius * radiusDist(rng);

                float x = (majorRadius + randomVariation * std::cos(v)) * std::cos(u);
                float z = (majorRadius + randomVariation * std::cos(v)) * std::sin(u);
                float y = randomVariation * std::sin(v) * verticalScale;

                float scale = scaleDist(rng);

                float rotX = rotDist(rng) + _asteroidTime * kRotationSpeed;
                float rotY = rotDist(rng) + _asteroidTime * kRotationSpeed;
                float rotZ = rotDist(rng) + _asteroidTime * kRotationSpeed;

                glm::mat4 T = glm::translate(glm::mat4(1.0f), glm::vec3(x, y, z));
                glm::mat4 R = glm::rotate(glm::mat4(1.0f), rotX, glm::vec3(1, 0, 0));
                R = glm::rotate(R, rotY, glm::vec3(0, 1, 0));
                R = glm::rotate(R, rotZ, glm::vec3(0, 0, 1));
                glm::mat4 S = glm::scale(glm::mat4(1.0f), glm::vec3(scale));

                it->second->addToDrawCommands(T * R * S, drawCommands);
            }
        }

        _asteroidTime -= 0.05f * _deltaTime;
        if (_asteroidTime < -glm::two_pi<float>())
        {
            _asteroidTime += glm::two_pi<float>();
        }
    }

    it = loadedAssets.find("planet");
    if (it != loadedAssets.end() && it->second)
    {
        glm::mat4 model = glm::scale(glm::mat4(1.0f), glm::vec3(2.0f));
        it->second->addToDrawCommands(model, drawCommands);
    }
}

void Scene::processSliderEvent()
{
    const Uint8* keys = SDL_GetKeyboardState(NULL);

    if (keys[SDL_SCANCODE_J])
    {
        numAsteroids -= _deltaTime * 5000;
        if (numAsteroids < kSliderMin)
        {
            numAsteroids = 0;
        }
    }
    if (keys[SDL_SCANCODE_K])
    {
        numAsteroids += _deltaTime * 5000;
        if (numAsteroids > kSliderMax)
        {
            numAsteroids = kSliderMax;
        }
    }
}

void Scene::cleanup()
{
    loadedAssets.clear();
}
