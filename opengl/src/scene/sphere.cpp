#include "scene/sphere.h"
#include "render/shader.h"
#include <glad/glad.h>
#include <glm/gtc/matrix_transform.hpp>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Sphere::Sphere(uint32_t sectorCount, uint32_t stackCount)
{
    // --- 1. Generate Sphere Vertices & Indices ---
    std::vector<float> vertices;
    std::vector<uint32_t> indices;

    float x, y, z, xy;                         // vertex position
    float nx, ny, nz, lengthInv = 1.0f / 1.0f; // vertex normal
    float s, t;                                // vertex texCoord

    float sectorStep = 2 * M_PI / sectorCount;
    float stackStep = M_PI / stackCount;
    float sectorAngle, stackAngle;

    for (uint32_t i = 0; i <= stackCount; ++i)
    {
        stackAngle = M_PI / 2 - i * stackStep;
        xy = 1.0f * cosf(stackAngle);
        z = 1.0f * sinf(stackAngle);

        for (uint32_t j = 0; j <= sectorCount; ++j)
        {
            sectorAngle = j * sectorStep;

            // vertex position
            x = xy * cosf(sectorAngle);
            y = xy * sinf(sectorAngle);
            vertices.push_back(x);
            vertices.push_back(y);
            vertices.push_back(z);

            // normalized vertex normal
            nx = x * lengthInv;
            ny = y * lengthInv;
            nz = z * lengthInv;
            vertices.push_back(nx);
            vertices.push_back(ny);
            vertices.push_back(nz);

            // vertex tex coord
            s = (float)j / sectorCount;
            t = (float)i / stackCount;
            vertices.push_back(s);
            vertices.push_back(t);
        }
    }

    // generate indices for triangles
    uint32_t k1, k2;
    for (uint32_t i = 0; i < stackCount; ++i)
    {
        k1 = i * (sectorCount + 1);
        k2 = k1 + sectorCount + 1;

        for (uint32_t j = 0; j < sectorCount; ++j, ++k1, ++k2)
        {
            if (i != 0)
            {
                indices.push_back(k1);
                indices.push_back(k2);
                indices.push_back(k1 + 1);
            }

            if (i != (stackCount - 1))
            {
                indices.push_back(k1 + 1);
                indices.push_back(k2);
                indices.push_back(k2 + 1);
            }
        }
    }
    m_indexCount = indices.size();

    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(float), &vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(uint32_t), &indices[0], GL_STATIC_DRAW);

    // Position attribute
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal attribute
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Texture coordinate attribute
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void Sphere::draw(Shader& shader, const glm::mat4& projection, const glm::mat4& view, const glm::vec3 lightPos)
{
    shader.use();
    shader.setMat4("projection", projection);
    shader.setMat4("view", view);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, lightPos);
    model = glm::scale(model, glm::vec3(0.1f));
    shader.setMat4("model", model);

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, m_indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}
