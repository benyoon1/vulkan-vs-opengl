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

// TODO: temporary fix shadow control
uniform int receiveShadow;
uniform int hasTexture;

// spotlight
uniform int spotEnabled;
uniform vec3 spotlightPos;
uniform vec3 spotlightDir;
uniform vec3 spotColor;
uniform float spotInnerCutoff; // cos(innerAngle)
uniform float spotOuterCutoff; // cos(outerAngle)
uniform float spotIntensity; // scalar intensity

vec3 calcSpotlight(
    vec3 sPos,
    vec3 sDir,
    vec3 sColor,
    float innerCut,
    float outerCut,
    float intensity,
    vec3 fragPos,
    vec3 normal,
    vec3 viewDir
) {
    vec3 N = normalize(normal);
    vec3 L = normalize(sPos - fragPos); // from frag to light
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

vec3 calcLight(vec3 lightPos, vec3 lightColor, vec3 fragPos, vec3 normal, vec3 viewDir) {
    // Diffuse
    vec3 norm = normalize(normal);
    vec3 lightDir = normalize(lightPos - fragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * lightColor;

    // Specular
    vec3 specular = vec3(0.0);
    if (
        diff >
        0.0 // This check prevents highlights on surfaces not facing the light
    ) {
        float specularStrength = 0.5;
        vec3 reflectDir = reflect(-lightDir, norm);
        float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
        specular = specularStrength * spec * lightColor;
    }

    return diffuse + specular;
}

float calcShadow(vec4 fragPosLightSpace, sampler2D shadowMap, vec3 lightPos) {
    // perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    // get closest depth value from light's perspective (using [0,1] range fragPosLight as coords)
    // get depth of current fragment from light's perspective
    float currentDepth = projCoords.z;
    // calculate bias (based on depth map resolution and slope)
    vec3 normal = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float bias = max(0.05 * (1.0 - dot(normal, lightDir)), 0.005);
    // PCF
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;
        }
    }
    shadow /= 9.0;

    if (projCoords.z > 1.0) shadow = 0.0;

    return shadow;
}

void main() {
    vec3 sunDirection = normalize(sunPos);
    float sunHeight = sunDirection.y;
    float fadeFactor = smoothstep(-0.02, 0.02, sunHeight);
    vec3 sunsetColor = vec3(1.0, 0.6, 0.2);
    float sunsetFactor = smoothstep(0.25, 0.0, sunHeight);
    vec3 dynamicLightColor = mix(sunColor, sunsetColor, sunsetFactor);

    vec3 viewDir = normalize(viewPos - FragPos);

    float sunShadow = 0.0;
    float spotlightShadow = 0.0;
    if (receiveShadow == 1) {
        sunShadow = calcShadow(FragPosSunLightSpace, sunShadowMapTextureNum, sunPos);
        spotlightShadow = calcShadow(FragPosSpotLightSpace, spotlightShadowMapTextureNum, spotlightPos);
    }

    float ambientStrength = 0.1;
    vec3 ambient = ambientStrength * vec3(0.8, 0.85, 0.95);

    vec3 sunResult = calcLight(sunPos, dynamicLightColor, FragPos, Normal, viewDir);

    vec3 spotlightResult = vec3(0.0);
    if (spotEnabled == 1) {
        spotlightResult = calcSpotlight(
            spotlightPos,
            spotlightDir,
            spotColor,
            spotInnerCutoff,
            spotOuterCutoff,
            spotIntensity,
            FragPos,
            Normal,
            viewDir
        );
    }

    vec3 sceneLighting = ambient + (1.0 - sunShadow) * sunResult * fadeFactor;
    sceneLighting += (1.0 - spotlightShadow) * spotlightResult; // spotlight shouldn't be affected by fadeFactor

    vec3 result = sceneLighting * objectColor;
    vec3 baseColor = hasTexture == 1 ? texture(texture_diffuse1, TexCoords).rgb : vec3(1.0);

    FragColor = vec4(baseColor * result, 1.0);
}
