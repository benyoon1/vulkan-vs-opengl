#ifndef ROBOT_ARM_H
#define ROBOT_ARM_H

#include "model.h"
#include <string>

class Camera;
class Shader;
class Model;

class RobotArm
{
public:
    RobotArm(const std::string& wristPath, const std::string& lowerArmPath, const std::string& upperArmPath);

    static constexpr glm::vec3 kUpperArmModelPos{1.0f, -0.5f, -1.1f};
    static constexpr glm::vec3 kLowerArmModelPos{-0.087621f, 0.891389f, -0.68283f};
    static constexpr glm::vec3 kWristModelPos{0.0063f, -0.5445f, -1.664f};
    static constexpr float kMuzzleOffset = -0.3f;

    // pose interface
    float getLowerArmAngle() const { return m_lowerArmAngle; }
    float getUpperArmAngle() const { return m_upperArmAngle; }
    float getWristAngle() const { return m_wristAngle; }
    void setLowerArmAngle(float deg) { m_lowerArmAngle = deg; }
    void setUpperArmAngle(float deg) { m_upperArmAngle = deg; }
    void setWristAngle(float deg) { m_wristAngle = deg; }

    void configureShader(Shader& shader) const;
    void draw(Shader& shader, const glm::mat4& projection, const glm::mat4& view, const Camera& camera,
              const glm::vec3 sunPos, const glm::vec3 spotlightPos);
    void drawShadowMap(Shader& depthShader, const glm::mat4& lightSpaceMatrix);
    glm::vec3 getSpotlightPos() const { return m_spotlightPos; }
    glm::vec3 getSpotlightDir() const { return m_spotlightDir; }
    void updateWristPose(Camera& camera);

private:
    // models
    Model m_wrist;
    Model m_lowerArm;
    Model m_upperArm;

    // joint angles
    float m_lowerArmAngle{0.0f};
    float m_upperArmAngle{0.0f};
    float m_wristAngle{0.0f};

    // spotlight pose
    glm::vec3 m_spotlightPos{0.0f, 0.0f, 0.0f};
    glm::vec3 m_spotlightDir{0.0f, 0.0f, -1.0f};

    // matrices
    glm::mat4 m_wristModel{1.0f};
    glm::mat4 m_lowerArmModel{1.0f};
    glm::mat4 m_upperArmModel{1.0f};
};

#endif