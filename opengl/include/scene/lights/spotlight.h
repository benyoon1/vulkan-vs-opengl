#ifndef SPOTLIGHT_H
#define SPOTLIGHT_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

class Spotlight
{
public:
    Spotlight();

    static constexpr float kOuterCutDeg{20.0f};
    static constexpr float kInnerCutDeg{12.0f};
    static constexpr float kSpotFov{glm::radians(kOuterCutDeg * 2.0f)};
    static constexpr float kSpotNear{1.0f};
    static constexpr float kSpotFar{2000.0f};
    static constexpr float kIntensity{100.0f};
    static constexpr glm::vec3 kSpotColor{1.0f, 0.98f, 0.90f};

    glm::mat4 getSpotLightSpaceMatrix() const { return m_spotLightSpace; }
    glm::mat4 getSpotLightProjection() const { return m_spotProj; }
    glm::mat4 getSpotLightView() const { return m_spotView; }

    void update();

private:
    glm::mat4 m_spotProj{1.0f};
    glm::mat4 m_spotView{1.0f};
    glm::mat4 m_spotLightSpace{1.0f};
};

#endif // SPOTLIGHT_H