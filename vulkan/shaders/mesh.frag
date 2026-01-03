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

vec3 calcSpotlight(vec3 sPos, vec3 sDir, vec3 sColor, float innerCut, float outerCut, float intensity, vec3 fragPos,
                   vec3 normal, vec3 viewDir)
{
    vec3 N = normalize(normal);
    vec3 L = normalize(sPos - fragPos);     // from frag to light
    float theta = dot(L, -normalize(sDir)); // angle vs spotlight axis

    // Soft edge between outer and inner cutoff
    float eps = max(innerCut - outerCut, 1e-4);
    float spot = clamp((theta - outerCut) / eps, 0.0, 1.0);

    // Lambert + Blinn/Phong spec
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = diff * sColor;

    vec3 reflectDir = reflect(-L, N);
    float spec = diff > 0.0 ? pow(max(dot(viewDir, reflectDir), 0.0), 32.0) : 0.0;
    vec3 specular = 0.5 * spec * sColor;

    // Distance attenuation
    float dist = length(sPos - fragPos);
    float attenuation = 1.0 / (1.0 + 0.7 * dist + 1.8 * dist * dist);

    return (diffuse + specular) * spot * attenuation * intensity;
}

vec3 calcLight(vec3 lightPos, vec3 lightColor, vec3 fragPos, vec3 normal, vec3 viewDir)
{
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

    // vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // vec2 uv = projCoords.xy * 0.5 + 0.5;  // map [-1,1] -> [0,1]

    // float closestDepth = texture(shadowMaps[shadowID], uv).r;
    // float currentDepth = projCoords.z;
    // float shadow = currentDepth > closestDepth  ? 1.0 : 0.0;

    return shadow;
}

void main()
{
    float sunHeight = normalize(sceneData.sunlightPosition).y;
    float fadeFactor = smoothstep(-0.04, 0.04, sunHeight);
    vec3 sunsetColor = vec3(1.0, 0.6, 0.2);
    float sunsetFactor = smoothstep(0.25, 0.0, sunHeight);
    vec3 sunColor = sceneData.sunlightColor.xyz;
    vec3 dynamicLightColor = mix(sunColor, sunsetColor, sunsetFactor);

    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * vec3(0.8, 0.85, 0.95);
    vec3 objectColor = vec3(0.85f, 0.553f, 0.133f);
    vec3 viewDir = normalize(sceneData.cameraPosition.xyz - inWorldPos);
    vec4 FragPosSunLightSpace = sceneData.sunlightViewProj * vec4(inWorldPos, 1.0);

    float sunShadow = calcShadow(FragPosSunLightSpace, sceneData.shadowParams.x, sceneData.sunlightPosition.xyz);
    vec3 sunResult = calcLight(sceneData.sunlightPosition.xyz, dynamicLightColor, inWorldPos, inNormal, viewDir);

    vec3 spotlightResult =
        calcSpotlight(sceneData.spotlightPos.xyz, sceneData.spotlightDir.xyz, sceneData.spotColor.xyz,
                      sceneData.spotCutoffAndIntensity.x, sceneData.spotCutoffAndIntensity.y,
                      sceneData.spotCutoffAndIntensity.z, inWorldPos, inNormal, viewDir);

    vec3 sceneLighting = ambient + (1.0 - sunShadow) * sunResult * fadeFactor + spotlightResult;

    vec3 result = sceneLighting * objectColor;

    int colorID = materialData.colorTexID;
    vec3 color = texture(allTextures[colorID], inUV).xyz;

    outFragColor = vec4(color * result, 1.0);
}
