#include "robotArm.h"
#include "camera.h"
#include <SDL_events.h>
#include <glm/gtc/matrix_transform.hpp>

void RobotArm::update(Camera& camera)
{
    m_upperArmModel = glm::mat4(1.0f);
    m_upperArmModel = glm::translate(m_upperArmModel, kUpperArmModelPos);
    m_upperArmModel = glm::rotate(m_upperArmModel, glm::radians(m_upperArmAngle), glm::vec3(1.0f, 0.0f, 0.0f));

    m_lowerArmModel = m_upperArmModel;
    m_lowerArmModel = glm::translate(m_lowerArmModel, kLowerArmModelPos);
    m_lowerArmModel = glm::rotate(m_lowerArmModel, glm::radians(m_lowerArmAngle), glm::vec3(1.0f, 0.0f, 0.0f));

    m_wristModel = m_lowerArmModel;
    m_wristModel = glm::translate(m_wristModel, kWristModelPos);
    m_wristModel = glm::rotate(m_wristModel, glm::radians(m_wristAngle), glm::vec3(1.0f, 0.0f, 0.0f));

    // convert from camera space (HUD) to world space using the camera view
    glm::mat4 camWorld = glm::inverse(camera.getViewMatrix());
    glm::mat4 wristWorld = camWorld * m_wristModel;
    glm::mat3 wristWorldRot = glm::mat3(wristWorld);

    glm::vec3 worldTip = glm::vec3(wristWorld * glm::vec4(0.0f, -0.0f, kMuzzleOffset, 1.0f));
    glm::vec3 worldForward = glm::normalize(wristWorldRot * glm::vec3(0.0f, 0.0f, -1.0f));
    m_spotlightPos = worldTip;
    m_spotlightDir = worldForward;
}

void RobotArm::processSDLEvent()
{
    const Uint8* keys = SDL_GetKeyboardState(NULL);

    if (keys[SDL_SCANCODE_I])
        m_upperArmAngle += 1.0f;
    if (keys[SDL_SCANCODE_K])
        m_upperArmAngle -= 1.0f;
    if (keys[SDL_SCANCODE_U])
        m_lowerArmAngle += 1.0f;
    if (keys[SDL_SCANCODE_J])
        m_lowerArmAngle -= 1.0f;
    if (keys[SDL_SCANCODE_O])
        m_wristAngle += 1.0f;
    if (keys[SDL_SCANCODE_L])
        m_wristAngle -= 1.0f;
}
