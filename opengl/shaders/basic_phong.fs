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

void main() {
    vec3 lightColor = vec3(1.0, 1.0, 1.0);
    vec3 viewDir = normalize(viewPos - FragPos);

    float ambientStrength = 0.3;
    vec3 ambient = ambientStrength * vec3(1.0);

    vec3 sunResult = calcLight(sunPos, lightColor, FragPos, Normal, viewDir);

    vec3 phongLighting = ambient + sunResult;

    vec4 texColor = hasTexture == 1 ? texture(texture_diffuse1, TexCoords) : vec4(1.0);
    if (texColor.a < 0.5) discard;

    FragColor = vec4(texColor.rgb * phongLighting, 1.0);
}
