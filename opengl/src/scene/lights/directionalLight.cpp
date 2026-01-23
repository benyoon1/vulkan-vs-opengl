#include "scene/lights/directionalLight.h"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

DirectionalLight::DirectionalLight()
{
    m_lastTime = glfwGetTime();
    m_lightProjection = glm::ortho(-kOrthoSize, kOrthoSize, -kOrthoSize, kOrthoSize, kNearPlane, kFarPlane);
}

void DirectionalLight::update(float sunSpeed)
{
    // accumulate time for sun rotation speedup
    double now = glfwGetTime();
    float dt = static_cast<float>(now - m_lastTime);
    m_simTime += dt * sunSpeed;
    m_lastTime = now;

    m_sunPos = glm::vec3(0.0f, 0.0f, 0.0f);
    m_sunDirection = glm::normalize(m_sunPos);
    m_lightView = glm::lookAt(m_sunPos, glm::vec3(0.0f), glm::vec3(0.0, 1.0, 0.0));
}

glm::mat4 DirectionalLight::getLightSpaceMatrix() const
{
    return m_lightProjection * m_lightView;
}