#include "render/shadowMap.h"
#include <glad/glad.h>

#include <iostream>

ShadowMap::ShadowMap()
{
    glGenFramebuffers(1, &m_fbo);

    // create depth texture
    glGenTextures(1, &m_texture);
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, kShadowWidth, kShadowHeight, 0, GL_DEPTH_COMPONENT, GL_FLOAT,
                 NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    float borderColor[] = {1.0, 1.0, 1.0, 1.0};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

    // attach depth texture as FBO's depth buffer
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, m_texture, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "ERROR::SHADOWMAP: Framebuffer not complete! status = 0x" << std::hex << status << std::dec
                  << std::endl;

        // restore GL state
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, 0);

        reset();

        throw std::runtime_error("ShadowMap: framebuffer not complete");
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

ShadowMap::~ShadowMap()
{
    reset();
}

void ShadowMap::reset()
{
    if (m_fbo)
    {
        glDeleteFramebuffers(1, &m_fbo);
        m_fbo = 0;
    }
    if (m_texture)
    {
        glDeleteTextures(1, &m_texture);
        m_texture = 0;
    }
}

void ShadowMap::bind()
{
    glViewport(0, 0, m_width, m_height);
    glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
    glClear(GL_DEPTH_BUFFER_BIT);
}

void ShadowMap::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void ShadowMap::bindTexture(uint32_t textureUnit)
{
    glActiveTexture(textureUnit);
    glBindTexture(GL_TEXTURE_2D, m_texture);
}
