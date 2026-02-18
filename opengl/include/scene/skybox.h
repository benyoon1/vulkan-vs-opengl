#ifndef SKYBOX_H
#define SKYBOX_H

#include <array>
#include <glm/glm.hpp>
#include <string>

class Camera;
class Shader;

class Skybox
{
public:
    Skybox();
    void draw(Shader& skyboxShader, const glm::mat4& projection, Camera& camera, const glm::vec3& sunDirection,
              const glm::vec2& screenSize);
    void loadCubemap(const std::array<std::string, 6>& faces);

private:
    uint32_t m_skyboxVAO{0};
    uint32_t m_cubeVBO{0};
    uint32_t m_cubemapTexture{0};

    static const float cubeVertices[108];
};

#endif // SKYBOX_H
