#version 450
#extension GL_GOOGLE_include_directive : require

#include "../v1/tonemapping.glsl"
#include "../v1/colorConversion.glsl"
#include "../v1/fog.glsl"
#include "../v1/linearDepth.glsl"

struct S_POSTPROC {
    mat4 mtxInvProj;
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
    S_POSTPROC ppdata;
};

layout(location = 0) out vec4 fragColor0;

void main() {

    vec4 inColor = texelFetch( samp_input, ivec2(gl_FragCoord.xy), 0 );
    float znorm = texelFetch( samp_depth, ivec2(gl_FragCoord.xy), 0 ).x;
    float linearZ = linearize_depth( znorm, ppdata.fZfar, ppdata.fZnear );
    float fogFactor = getFogFactor( ppdata.sFogParams, abs( linearZ ) );

    vec4 ndc = vec4(texcoord * 2.0 - 1.0, znorm, 1.0);
    ndc.y = -ndc.y;
    vec4 posWS = ppdata.mtxInvProj * ndc;
    posWS.xyz /= posWS.w;

    fogFactor = mix(1.0, fogFactor, znorm > 0.0);
    inColor.rgb = ACESFitted( mix( ppdata.sFogParams.color, ppdata.fExposure * inColor.rgb, fogFactor ) );
    inColor.rgb = linearTosRGB( inColor.rgb );
    fragColor0 = vec4( inColor.rgb, 1.0 );
}