#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 5) in mat4 instanceModel;

uniform mat4 lightSpaceMatrix;

void main()
{
    gl_Position = lightSpaceMatrix * instanceModel * vec4(aPos, 1.0);
}
