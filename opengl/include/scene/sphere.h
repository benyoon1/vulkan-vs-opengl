#ifndef SPHERE_H
#define SPHERE_H

#include <cstdint>
#include <glm/glm.hpp>

class Shader;

class Sphere
{
public:
    Sphere(uint32_t sectorCount, uint32_t stackCount);
    void draw(Shader& shader, const glm::mat4& projection, const glm::mat4& view, const glm::vec3 lightPos);

private:
    uint32_t m_VAO{0};
    uint32_t m_VBO{0};
    uint32_t m_EBO{0};
    uint32_t m_indexCount{0};
};

#endif // SPHERE_H
