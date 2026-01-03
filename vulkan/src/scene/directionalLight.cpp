#include "directionalLight.h"
#include "SDL_timer.h"
#include <glm/gtc/matrix_transform.hpp>

DirectionalLight::DirectionalLight()
{
    m_lastTime = SDL_GetTicks64() / 1000.0;
    m_lightProjection = glm::ortho(-kOrthoSize, kOrthoSize, -kOrthoSize, kOrthoSize, kFarPlane, kNearPlane);
}

void DirectionalLight::update()
{
    // accumulate time for sun rotation speedup
    double now = SDL_GetTicks64() / 1000.0;
    float dt = static_cast<float>(now - m_lastTime);
    m_simTime += dt * m_sunSpeed;
    m_lastTime = now;

    m_sunPos = glm::vec3(0.0f, cos(m_simTime) * m_sunRadius, sin(m_simTime) * m_sunRadius);
    m_sunDirection = glm::normalize(m_sunPos);
    m_lightView = glm::lookAt(m_sunPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
}

glm::mat4 DirectionalLight::getLightSpaceMatrix() const
{
    return m_lightProjection * m_lightView;
}