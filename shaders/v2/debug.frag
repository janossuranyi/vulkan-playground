#version 450

layout(location = 0) out vec4 out_Color0;
layout(location = 0) in vec3 in_Color0;
layout(location = 1) in vec2 in_Texcoord0;

layout(set = 0, binding = 0) uniform texture2D t_BaseColor;
layout(set = 0, binding = 128) uniform sampler s_Color;

void main()
{
    out_Color0 = vec4(in_Color0, 1.0) * texture(sampler2D(t_BaseColor, s_Color), in_Texcoord0);
}