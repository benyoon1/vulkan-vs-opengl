#ifndef SHADER_H
#define SHADER_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <string>

class Shader
{
public:
    Shader(const char* vertex_path, const char* fragment_path);
    ~Shader();

    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;
    Shader(Shader&&) = default;
    Shader& operator=(Shader&&) = default;

    void use() const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setVec3(const std::string& name, const glm::vec3& value) const;
    void setVec2(const std::string& name, const glm::vec2& value) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;

private:
    uint32_t m_id{0};
    void checkCompileErrors(GLuint shader, std::string type);
};

#endif
