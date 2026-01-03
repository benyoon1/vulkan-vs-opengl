#ifndef SKYBOX_H
#define SKYBOX_H

#include <glm/glm.hpp>

class Camera;
class Shader;

class Skybox
{
public:
    Skybox();
    void draw(Shader& skyboxShader, const glm::mat4& projection, Camera& camera, const glm::vec3& sunDirection,
              const glm::vec2& screenSize);

private:
    uint32_t m_skyboxVAO{0};
    uint32_t m_cubeVBO{0};

    static const float cubeVertices[108];
};

#endif // SKYBOX_H
