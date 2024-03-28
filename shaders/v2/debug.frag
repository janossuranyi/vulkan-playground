#version 450

layout(location = 0) out vec4 outColor0;
layout(location = 0) in vec3 color0;

layout(set = 0, binding = 0) uniform sampler2D images[1024];

void main(){
    outColor0 = vec4(color0, 1.0);
}