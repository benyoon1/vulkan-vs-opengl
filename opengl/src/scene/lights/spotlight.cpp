#include "scene/lights/spotlight.h"
#include "scene/robotArm.h"

Spotlight::Spotlight()
{
    // hard coded spotlight pos
    // glm::vec3 spotlightPos = glm::vec3(200.0f, 100.0f, 400.0f);
    // glm::vec3 spotlightDir = glm::normalize(glm::vec3(300.0f, 100.0f, 400.0f) - spotlightPos);

    m_spotProj = glm::perspective(kSpotFov, 1.0f, kSpotNear, kSpotFar);
}

void Spotlight::update(const RobotArm& robotArm)
{
    m_spotView = glm::lookAt(robotArm.getSpotlightPos(), robotArm.getSpotlightPos() + robotArm.getSpotlightDir(),
                             glm::vec3(0, 1, 0));
    m_spotLightSpace = m_spotProj * m_spotView;
}