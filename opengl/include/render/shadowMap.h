#ifndef SHADOW_MAP_H
#define SHADOW_MAP_H

#include <cstdint>

class ShadowMap
{
public:
    ShadowMap();
    ~ShadowMap();

    static constexpr uint32_t kShadowWidth{2048};
    static constexpr uint32_t kShadowHeight{2048};
    static constexpr uint8_t kSunShadowTextureNum{1};
    static constexpr uint8_t kSpotShadowTextureNum{2};

    void reset();
    void bind();
    void unbind();
    void bindTexture(uint32_t textureUnit);

    uint32_t getFBO() const { return m_fbo; }
    uint32_t getTexture() const { return m_texture; }
    uint32_t getWidth() const { return m_width; }
    uint32_t getHeight() const { return m_height; }

private:
    uint32_t m_fbo{0};
    uint32_t m_texture{0};
    uint32_t m_width{kShadowWidth};
    uint32_t m_height{kShadowHeight};
};

#endif // SHADOW_MAP_H