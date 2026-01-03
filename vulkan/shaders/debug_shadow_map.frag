#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require

#define USE_BINDLESS
#include "input_structures.glsl"

layout(location = 0) in vec2 TexCoords;

layout(location = 0) out vec4 outFragColor;

void main()
{
    float depthValue = texture(shadowMaps[sceneData.shadowParams.x], TexCoords).r;
    outFragColor = vec4(vec3(clamp(depthValue, 0.0, 1.0)), 1.0);
}
