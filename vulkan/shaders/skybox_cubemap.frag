#version 450

layout(location = 0) in vec3 TexCoords;
layout(location = 0) out vec4 FragColor;

layout(set = 0, binding = 0) uniform samplerCube skyboxTexture;

void main()
{
    FragColor = texture(skyboxTexture, TexCoords);
}
