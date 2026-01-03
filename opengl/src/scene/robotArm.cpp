#include "scene/robotArm.h"
#include "core/utils.h"
#include "render/shader.h"
#include "scene/camera.h"
#include <glm/gtc/matrix_transform.hpp>

RobotArm::RobotArm(const std::string& wristPath, const std::string& lowerArmPath, const std::string& upperArmPath)
    : m_wrist(wristPath), m_lowerArm(lowerArmPath), m_upperArm(upperArmPath)
{
}

void RobotArm::configureShader(Shader& shader) const
{
    shader.setVec3("objectColor", glm::vec3(1.0f));
    shader.setInt("receiveShadow", 0);
}

void RobotArm::draw(Shader& shader, const glm::mat4& projection, const glm::mat4& view, const Camera& camera,
                    const glm::vec3 sunPos, const glm::vec3 spotlightPos)
{
    shader.use();
    shader.setMat4("projection", projection);
    glm::mat4 camWorld = glm::inverse(view); // camera's world transform

    shader.setMat4("model", camWorld * m_upperArmModel);
    m_upperArm.draw(shader, projection, view, camera, sunPos, spotlightPos);

    shader.setMat4("model", camWorld * m_lowerArmModel);
    m_lowerArm.draw(shader, projection, view, camera, sunPos, spotlightPos);

    shader.setMat4("model", camWorld * m_wristModel);
    m_wrist.draw(shader, projection, view, camera, sunPos, spotlightPos);
}

void RobotArm::drawShadowMap(Shader& depthShader, const glm::mat4& lightSpaceMatrix)
{
    m_upperArm.drawShadowMap(depthShader, lightSpaceMatrix, m_upperArmModel);
    m_lowerArm.drawShadowMap(depthShader, lightSpaceMatrix, m_lowerArmModel);
    m_wrist.drawShadowMap(depthShader, lightSpaceMatrix, m_wristModel);
}

void RobotArm::updateWristPose(Camera& camera)
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
