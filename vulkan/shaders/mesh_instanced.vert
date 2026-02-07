#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout(location = 0) out vec3 outNormal;
layout(location = 1) out vec3 outWorldPos;
layout(location = 2) out vec2 outUV;

struct Vertex
{
    vec3 position;
    float uv_x;
    vec3 normal;
    float uv_y;
    vec4 color;
};

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
    Vertex vertices[];
};

layout(buffer_reference, std430) readonly buffer InstanceBuffer
{
    mat4 transforms[];
};

layout(push_constant) uniform constants
{
    mat4 viewProj;
    VertexBuffer vertexBuffer;
    InstanceBuffer instanceBuffer;
}
PushConstants;

void main()
{
    Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
    mat4 render_matrix = PushConstants.instanceBuffer.transforms[gl_InstanceIndex];

    vec4 position = vec4(v.position, 1.0f);

    gl_Position = PushConstants.viewProj * render_matrix * position;

    outNormal = (render_matrix * vec4(v.normal, 0.f)).xyz;
    outWorldPos = (render_matrix * position).xyz;
    outUV.x = v.uv_x;
    outUV.y = v.uv_y;
}
