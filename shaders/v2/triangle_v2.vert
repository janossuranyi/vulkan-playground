#version 450

layout(location = 0) in vec3 inPosition;

layout(set = 0, binding = 0) uniform uPassData {
    mat4 mtxView;
    mat4 mtxProjection;
};

layout(set = 0, binding = 1) uniform uDrawData {
    mat4 mtxModel;
    vec4 color;
};

layout(location = 0) out vec4 outColor;
layout(location = 1) out vec3 outFragCoordVS;
layout(location = 2) out vec3 outLightVS;

void main() {
    vec3 light = normalize(vec3(8,10,7));
    mat4 mvp = mtxProjection * mtxView * mtxModel;
    vec4 posVS = (mtxView * mtxModel) * vec4(inPosition, 1.0);
    gl_Position = mvp * vec4(inPosition, 1.0);
    gl_Position.y = -gl_Position.y;
    outColor = color;
    outFragCoordVS = posVS.xyz;
    outLightVS = (mtxView * vec4(light,0.0)).xyz;
}
