#version 450
#extension GL_GOOGLE_include_directive : require

#include "../v1/tonemapping.glsl"
#include "../v1/colorConversion.glsl"
#include "../v1/fog.glsl"
#include "../v1/linearDepth.glsl"

struct S_POSTPROC {
    FogParameters sFogParams;
    float fExposure;
    float fZnear;
    float fZfar;
    int pad;
};

layout(location = 0) in vec2 texcoord;

layout(set = 0, binding = 0) uniform sampler2D samp_input;  
layout(set = 0, binding = 1) uniform sampler2D samp_depth;
layout(set = 0, binding = 2) uniform ubo_PostProcessData {
    S_POSTPROC pp;
};

layout(location = 0) out vec4 fragColor0;

void main() {
    vec4 inColor = textureLod( samp_input, texcoord, 0.0 );
    float znorm = textureLod( samp_depth, texcoord, 0.0 ).x;
    float linearZ = linearize_depth( znorm, pp.fZnear, pp.fZfar );
    float fogFactor = getFogFactor( pp.sFogParams, abs( linearZ ) );

    fogFactor = mix( fogFactor, 1.0, znorm == 1.0 );
    inColor.rgb = ACESFitted( mix( pp.sFogParams.color, pp.fExposure * inColor.rgb, fogFactor ) );
    inColor.rgb = linearTosRGB( inColor.rgb );
    fragColor0 = vec4( inColor.rgb, 1.0 );
}