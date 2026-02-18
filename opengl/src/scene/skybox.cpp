#include "scene/skybox.h"
#include "render/shader.h"
#include "scene/camera.h"
#include <glad/glad.h>
#include <iostream>
#include <stb_image.h>

const float Skybox::cubeVertices[108] = {
    // positions
    -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f,
    1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f,

    -1.0f, -1.0f, 1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  -1.0f,
    -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f, 1.0f,

    1.0f,  -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f,

    -1.0f, -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f, -1.0f, 1.0f,

    -1.0f, 1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  1.0f,
    1.0f,  1.0f,  1.0f,  -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f,  -1.0f,

    -1.0f, -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, -1.0f,
    1.0f,  -1.0f, -1.0f, -1.0f, -1.0f, 1.0f,  1.0f,  -1.0f, 1.0f};

Skybox::Skybox()
{
    glGenVertexArrays(1, &m_skyboxVAO);
    glGenBuffers(1, &m_cubeVBO);

    glBindVertexArray(m_skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
}

void Skybox::loadCubemap(const std::array<std::string, 6>& faces)
{
    if (m_cubemapTexture)
    {
        glDeleteTextures(1, &m_cubemapTexture);
        m_cubemapTexture = 0;
    }

    if (faces[0].empty())
        return;

    glGenTextures(1, &m_cubemapTexture);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTexture);

    stbi_set_flip_vertically_on_load(false);
    for (int i = 0; i < 6; ++i)
    {
        int width, height, nrChannels;
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data)
        {
            GLenum format = (nrChannels == 4) ? GL_RGBA : GL_RGB;
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE,
                         data);
            stbi_image_free(data);
        }
        else
        {
            std::cerr << "Cubemap face failed to load: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
}

void Skybox::draw(Shader& skyboxShader, const glm::mat4& projection, Camera& camera, const glm::vec3& sunDirection,
                  const glm::vec2& screenSize)
{
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    skyboxShader.use();

    glm::mat4 rotationOnlyView = glm::mat4(glm::mat3(camera.getViewMatrix()));

    skyboxShader.setMat4("projection", projection);
    skyboxShader.setMat4("view", rotationOnlyView);

    if (m_cubemapTexture)
    {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, m_cubemapTexture);
        skyboxShader.setInt("skybox", 0);
    }
    else
    {
        glm::mat4 invProjection = glm::inverse(projection);
        glm::mat4 invView = glm::inverse(rotationOnlyView);
        skyboxShader.setMat4("u_inverseProjection", invProjection);
        skyboxShader.setMat4("u_inverseView", invView);
        skyboxShader.setVec3("u_sunDirection", sunDirection);
        skyboxShader.setVec2("u_screenSize", screenSize);
    }

    glBindVertexArray(m_skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
}
