#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform uPassData {
    mat4 mtxView;
    mat4 mtxProjection;
};

layout(set = 0, binding = 1) uniform uDrawData {
    mat4 mtxModel;
};

void main() {
    mat4 mvp = mtxProjection * mtxView * mtxModel;
    gl_Position = mvp * vec4(inPosition, 1.0);
    gl_Position.y = -gl_Position.y;
}
