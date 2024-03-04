#ifndef PASSDATA_INC_
#define PASSDATA_INC_

struct S_PASS {
    mat4 mtxView;
    mat4 mtxProjection;
    vec4 vScaleBias;
    vec4 avSSAOkernel[12];
    vec4 vLightPos;
    vec4 vLightColor;
    vec4 vParams;
};

#endif