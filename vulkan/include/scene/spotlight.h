#ifndef SPOTLIGHT_H
#define SPOTLIGHT_H

#include <glm/glm.hpp>

struct SpotlightConstants
{
    static constexpr float kOuterCutDeg{20.0f};
    static constexpr float kInnerCutDeg{12.0f};
    static constexpr float kIntensity{100.0f};
    static constexpr glm::vec3 kSpotColor{1.0f, 0.98f, 0.90f};
};

struct SpotlightState
{
    float spotGain = 1.0f;
};

#endif // SPOTLIGHT_H