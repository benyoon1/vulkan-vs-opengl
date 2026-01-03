#include "scene/skybox.h"
#include "render/shader.h"
#include "scene/camera.h"
#include <glad/glad.h>

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

void Skybox::draw(Shader& skyboxShader, const glm::mat4& projection, Camera& camera, const glm::vec3& sunDirection,
                  const glm::vec2& screenSize)
{
    glDepthFunc(GL_LEQUAL);
    glDisable(GL_CULL_FACE);

    skyboxShader.use();

    glm::mat4 rotationOnlyView = glm::mat4(glm::mat3(camera.getViewMatrix()));

    skyboxShader.setMat4("projection", projection);
    skyboxShader.setMat4("view", rotationOnlyView);

    glm::mat4 invProjection = glm::inverse(projection);
    glm::mat4 invView = glm::inverse(rotationOnlyView);
    skyboxShader.setMat4("u_inverseProjection", invProjection);
    skyboxShader.setMat4("u_inverseView", invView);

    skyboxShader.setVec3("u_sunDirection", sunDirection);

    skyboxShader.setVec2("u_screenSize", screenSize);

    glBindVertexArray(m_skyboxVAO);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);

    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
}
