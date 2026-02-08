#include "scene/mesh.h"
#include <glad/glad.h>
#include <render/shader.h>

Mesh::Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, std::vector<Texture> textures)
{
    this->m_vertices = vertices;
    this->m_indices = indices;
    this->m_textures = textures;

    // setup mesh
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &m_VBO);
    glGenBuffers(1, &m_EBO);

    glBindVertexArray(m_VAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_VBO);
    glBufferData(GL_ARRAY_BUFFER, m_vertices.size() * sizeof(Vertex), &m_vertices[0], GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, m_indices.size() * sizeof(uint32_t), &m_indices[0], GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, texCoords));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, tangent));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, bitangent));

    glBindVertexArray(0);
}

Mesh::~Mesh()
{
    if (m_instanceVBO)
    {
        glDeleteBuffers(1, &m_instanceVBO);
    }
    if (m_VAO)
    {
        glDeleteVertexArrays(1, &m_VAO);
    }
    if (m_VBO)
    {
        glDeleteBuffers(1, &m_VBO);
    }
    if (m_EBO)
    {
        glDeleteBuffers(1, &m_EBO);
    }
}

void Mesh::draw(Shader& shader)
{
    // Check if this mesh has a diffuse texture
    bool hasDiffuse = false;
    for (const auto& tex : m_textures)
    {
        if (tex.type == "texture_diffuse")
        {
            hasDiffuse = true;
            break;
        }
    }
    shader.setInt("hasTexture", hasDiffuse ? 1 : 0);

    uint32_t diffuseNr = 1;
    uint32_t specularNr = 1;
    uint32_t normalNr = 1;
    uint32_t heightNr = 1;
    for (uint32_t i = 0; i < m_textures.size(); i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        std::string number;
        std::string name = m_textures[i].type;
        if (name == "texture_diffuse")
            number = std::to_string(diffuseNr++);
        else if (name == "texture_specular")
            number = std::to_string(specularNr++);
        else if (name == "texture_normal")
            number = std::to_string(normalNr++);
        else if (name == "texture_height")
            number = std::to_string(heightNr++);

        shader.setInt(name + number, static_cast<int>(i));
        glBindTexture(GL_TEXTURE_2D, m_textures[i].id);
    }

    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, static_cast<uint32_t>(m_indices.size()), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
}

void Mesh::setupInstanceBuffer(uint32_t maxInstances)
{
    glBindVertexArray(m_VAO);

    glGenBuffers(1, &m_instanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, maxInstances * sizeof(glm::mat4), nullptr, GL_DYNAMIC_DRAW);

    // mat4 = 4 consecutive vec4 attributes at locations 5, 6, 7, 8
    for (int i = 0; i < 4; i++)
    {
        GLuint loc = 5 + i;
        glEnableVertexAttribArray(loc);
        glVertexAttribPointer(loc, 4, GL_FLOAT, GL_FALSE, sizeof(glm::mat4),
                              (void*)(i * sizeof(glm::vec4)));
        glVertexAttribDivisor(loc, 1);
    }

    glBindVertexArray(0);
}

void Mesh::updateInstanceData(const glm::mat4* data, uint32_t count)
{
    glBindBuffer(GL_ARRAY_BUFFER, m_instanceVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, count * sizeof(glm::mat4), data);
}

void Mesh::drawInstanced(Shader& shader, uint32_t instanceCount)
{
    bool hasDiffuse = false;
    for (const auto& tex : m_textures)
    {
        if (tex.type == "texture_diffuse")
        {
            hasDiffuse = true;
            break;
        }
    }
    shader.setInt("hasTexture", hasDiffuse ? 1 : 0);

    uint32_t diffuseNr = 1;
    uint32_t specularNr = 1;
    uint32_t normalNr = 1;
    uint32_t heightNr = 1;
    for (uint32_t i = 0; i < m_textures.size(); i++)
    {
        glActiveTexture(GL_TEXTURE0 + i);
        std::string number;
        std::string name = m_textures[i].type;
        if (name == "texture_diffuse")
            number = std::to_string(diffuseNr++);
        else if (name == "texture_specular")
            number = std::to_string(specularNr++);
        else if (name == "texture_normal")
            number = std::to_string(normalNr++);
        else if (name == "texture_height")
            number = std::to_string(heightNr++);

        shader.setInt(name + number, static_cast<int>(i));
        glBindTexture(GL_TEXTURE_2D, m_textures[i].id);
    }

    glBindVertexArray(m_VAO);
    glDrawElementsInstanced(GL_TRIANGLES, static_cast<uint32_t>(m_indices.size()), GL_UNSIGNED_INT, 0,
                            instanceCount);
    glBindVertexArray(0);
    glActiveTexture(GL_TEXTURE0);
}

Mesh::Mesh(Mesh&& other) noexcept
    : m_vertices(std::move(other.m_vertices)), m_indices(std::move(other.m_indices)),
      m_textures(std::move(other.m_textures)), m_VAO(other.m_VAO), m_VBO(other.m_VBO), m_EBO(other.m_EBO),
      m_instanceVBO(other.m_instanceVBO)
{
    other.m_VAO = 0;
    other.m_VBO = 0;
    other.m_EBO = 0;
    other.m_instanceVBO = 0;
}

Mesh& Mesh::operator=(Mesh&& other) noexcept
{
    if (this != &other)
    {
        if (m_instanceVBO)
        {
            glDeleteBuffers(1, &m_instanceVBO);
        }
        if (m_VAO)
        {
            glDeleteVertexArrays(1, &m_VAO);
        }
        if (m_VBO)
        {
            glDeleteBuffers(1, &m_VBO);
        }
        if (m_EBO)
        {
            glDeleteBuffers(1, &m_EBO);
        }

        m_vertices = std::move(other.m_vertices);
        m_indices = std::move(other.m_indices);
        m_textures = std::move(other.m_textures);
        m_VAO = other.m_VAO;
        m_VBO = other.m_VBO;
        m_EBO = other.m_EBO;
        m_instanceVBO = other.m_instanceVBO;

        other.m_VAO = 0;
        other.m_VBO = 0;
        other.m_EBO = 0;
        other.m_instanceVBO = 0;
    }
    return *this;
}