#version 450

layout(push_constant) uniform constants
{
    mat4 invViewProj;
}
pc;

layout(location = 0) out vec3 TexCoords;

void main()
{
    vec2 uv = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec2 ndc = uv * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);

    // transform NDC back to world-space direction for cubemap sampling
    vec4 worldDir = pc.invViewProj * vec4(ndc, 1.0, 1.0);
    TexCoords = worldDir.xyz / worldDir.w;
}
