#version 330 core
out vec4 FragColor;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;
in vec4 FragPosSunLightSpace;
in vec4 FragPosSpotLightSpace;

uniform vec3 viewPos;
uniform vec3 objectColor;

// textures
uniform sampler2D texture_diffuse1;
uniform sampler2D sunShadowMapTextureNum;
uniform sampler2D spotlightShadowMapTextureNum;

// sun
uniform vec3 sunPos;
uniform vec3 sunColor;

uniform int hasTexture;
uniform int useShadowMap;

vec3 calcLight(vec3 lightPos, vec3 lightColor, vec3 fragPos, vec3 normal, vec3 viewDir) {
    // Diffuse
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    vec3 specular = vec3(0.0);
    // This check prevents highlights on surfaces not facing the light
    if (diff > 0.0) {
        float specularStrength = 0.5;
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        specular = specularStrength * spec * lightColor;
    }

    return diffuse + specular;
}

float calcShadow(vec4 fragPosLightSpace, sampler2D shadowMap, vec3 lightPos) {
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;
    float currentDepth = projCoords.z;

    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);

    // PCF 3x3
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    if (projCoords.z > 1.0)
        shadow = 0.0;

    return shadow;
}

void main() {
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 viewDir = normalize(viewPos - FragPos);

    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * vec3(1.0);

    vec3 sunResult = calcLight(sunPos, lightColor, FragPos, Normal, viewDir);

    float sunShadow = 0.0;
    if (useShadowMap == 1) {
        sunShadow = calcShadow(FragPosSunLightSpace, sunShadowMapTextureNum, sunPos);
    }

    vec3 phongLighting = ambient + (1.0 - sunShadow) * sunResult;

    vec4 texColor = hasTexture == 1 ? texture(texture_diffuse1, TexCoords) : vec4(1.0);
    if (texColor.a < 0.5) discard;

    FragColor = vec4(texColor.rgb * phongLighting, 1.0);
}
