#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_nonuniform_qualifier : require
// #extension GL_EXT_debug_printf : require

#define USE_BINDLESS
#include "input_structures.glsl"

layout(location = 0) in vec3 inNormal;
layout(location = 1) in vec3 inWorldPos;
layout(location = 2) in vec2 inUV;

layout(location = 0) out vec4 outFragColor;

vec3 calcLight(vec3 lightPos, vec3 fragPos, vec3 normal, vec3 viewDir)
{
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    // Diffuse
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    vec3 specular = vec3(0.0);
    if (diff > 0.0 // This check prevents highlights on surfaces not facing the light
    )
    {
        float specularStrength = 0.5;
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        specular = specularStrength * spec * lightColor;
    }

    return diffuse + specular;
}

void main()
{
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * vec3(1.0);
    vec3 viewDir = normalize(sceneData.cameraPosition.xyz - inWorldPos);

    vec3 sunResult = calcLight(sceneData.sunlightPosition.xyz, inWorldPos, inNormal, viewDir);

    vec3 phongLighting = ambient + sunResult;

    int colorID = materialData.colorTexID;
    vec4 color = texture(allTextures[colorID], inUV);
    if (color.a < 0.5)
        discard;

    outFragColor = vec4(color.rgb * phongLighting, 1.0);
}
