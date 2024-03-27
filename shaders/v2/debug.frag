#version 450

layout(location = 0) out vec4 outColor0;
layout(location = 0) in vec3 color0;

void main(){
    outColor0 = vec4(color0, 1.0);
}