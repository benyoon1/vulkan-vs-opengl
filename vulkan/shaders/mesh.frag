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
    vec3 lightColor = vec3(1.0, 1.0, 1.0); // white light
    // Diffuse
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    vec3 specular = vec3(0.0);
    // This check prevents highlights on surfaces not facing the light
    if (diff > 0.0)
    {
        float specularStrength = 0.5;
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        specular = specularStrength * spec * lightColor;
    }

    return diffuse + specular;
}

float calcShadow(vec4 fragPosLightSpace, uint shadowID, vec3 lightPos)
{
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    vec2 uv = projCoords.xy * 0.5 + 0.5; // map [-1,1] -> [0,1]
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z; // For Vulkan (GLM_FORCE_DEPTH_ZERO_TO_ONE), Z is already in [0,1].
    // calculate bias (based on depth map resolution and slope)
    vec3 normal = normalize(inNormal);
    vec3 lightDir = normalize(lightPos - inWorldPos);
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMaps[shadowID], 0);
    for (int x = -1; x <= 1; ++x)
    {
        for (int y = -1; y <= 1; ++y)
        {
            float pcfDepth = texture(shadowMaps[shadowID], uv + vec2(x, y) * texelSize).r;
            // shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
            shadow += currentDepth + bias < pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    if (projCoords.z > 1.0)
        shadow = 0.0;

    return shadow;
}

void main()
{
    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * vec3(1.0);
    vec3 viewDir = normalize(sceneData.cameraPosition.xyz - inWorldPos);
    vec4 FragPosSunLightSpace = sceneData.sunlightViewProj * vec4(inWorldPos, 1.0);

    float sunShadow = 0.0;
    // shadowParmams.y for shadow map on/off, shadowParams.x for shadow map index
    if (sceneData.shadowParams.y != 0u)
        sunShadow = calcShadow(FragPosSunLightSpace, sceneData.shadowParams.x, sceneData.sunlightPosition.xyz);
    vec3 diffSpec = calcLight(sceneData.sunlightPosition.xyz, inWorldPos, inNormal, viewDir);

    vec3 sceneLighting = ambient + (1.0 - sunShadow) * diffSpec;

    int colorID = materialData.colorTexID;
    vec4 color = texture(allTextures[colorID], inUV);
    if (color.a < 0.5)
        discard;

    outFragColor = vec4(color.rgb * sceneLighting, 1.0);
}
