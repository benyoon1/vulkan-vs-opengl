#ifndef MESH_H
#define MESH_H

#include <glm/glm.hpp>
#include <string>
#include <vector>

class Shader;

struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
    glm::vec2 texCoords;
    glm::vec3 tangent;
    glm::vec3 bitangent;
};

struct Texture
{
    uint32_t id;
    std::string type;
    std::string path;
};

class Mesh
{
public:
    Mesh(std::vector<Vertex> vertices, std::vector<uint32_t> indices, std::vector<Texture> textures);
    ~Mesh();

    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;
    Mesh(Mesh&& other) noexcept;
    Mesh& operator=(Mesh&& other) noexcept;

    void draw(Shader& shader);
    void setupInstanceBuffer(uint32_t maxInstances);
    void updateInstanceData(const glm::mat4* data, uint32_t count);
    void drawInstanced(Shader& shader, uint32_t instanceCount);
    uint32_t indexCount() const { return static_cast<uint32_t>(m_indices.size()); }

private:
    std::vector<Vertex> m_vertices;
    std::vector<uint32_t> m_indices;
    std::vector<Texture> m_textures;
    uint32_t m_VAO{0};
    uint32_t m_VBO{0};
    uint32_t m_EBO{0};
    uint32_t m_instanceVBO{0};
};

#endif
