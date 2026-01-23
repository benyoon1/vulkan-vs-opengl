#ifndef DIRECTIONAL_LIGHT_H
#define DIRECTIONAL_LIGHT_H

#include <glm/glm.hpp>

class DirectionalLight
{
public:
    DirectionalLight();

    static constexpr float kNearPlane{1.0f};
    static constexpr float kFarPlane{400.0f};
    static constexpr float kOrthoSize{100.0f};
    static constexpr glm::vec3 kSunColor{1.0f, 1.0f, 1.0f};

    void update(float sunSpeed);
    glm::vec3 getSunPosition() const { return m_sunPos; }
    glm::vec3 getSunDirection() const { return m_sunDirection; }
    glm::mat4 getLightSpaceMatrix() const;
    glm::mat4 getLightProjection() const { return m_lightProjection; }
    glm::mat4 getLightView() const { return m_lightView; }

private:
    float m_simTime{0.0f};
    double m_lastTime{0.0f};
    glm::vec3 m_sunPos{0.0f};
    glm::vec3 m_sunDirection{0.0f};
    glm::mat4 m_lightProjection{1.0f};
    glm::mat4 m_lightView{1.0f};
};

#endif // DIRECTIONAL_LIGHT_H
