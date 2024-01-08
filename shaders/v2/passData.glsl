#ifndef PASSDATA_INC_
#define PASSDATA_INC_

layout(set = 0, binding = 0) uniform uPassData {
    mat4 mtxView;
    mat4 mtxProjection;
    vec4 vScaleBias;
    vec4 avSSAOkernel[12];
    vec4 vLightPos;
    vec4 vLightColor;
};

#endif