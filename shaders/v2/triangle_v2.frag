#version 450

layout(location = 0) out vec4 outColor0;

void main() {
    outColor0.rbg = vec3(0.7);
    outColor0.a = 1.0;
}