#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec3 aNormal;
layout (location = 2) in vec2 aTexCoords;

out vec2 TexCoords;
out vec3 FragPos;
out vec3 Normal;
out vec4 FragPosSunLightSpace;
out vec4 FragPosSpotLightSpace;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform mat4 sunLightSpaceMatrix;
uniform mat4 spotLightSpaceMatrix;

void main()
{
    TexCoords = aTexCoords;
    FragPos = vec3(model * vec4(aPos, 1.0));
    Normal = mat3(transpose(inverse(model))) * aNormal;

    FragPosSunLightSpace = sunLightSpaceMatrix * vec4(FragPos, 1.0);
    FragPosSpotLightSpace = spotLightSpaceMatrix * vec4(FragPos, 1.0);

    gl_Position = projection * view * model * vec4(aPos, 1.0);
}
