#version 450

layout(location = 0) out vec4 out_Color0;
layout(location = 0) in vec3 in_Color0;

void main()
{
    out_Color0 = vec4(in_Color0, 1.0);
}