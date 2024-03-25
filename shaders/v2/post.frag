#version 450
#extension GL_GOOGLE_include_directive : require

#include "../v1/tonemapping.glsl"
#include "../v1/colorConversion.glsl"
#include "../v1/fog.glsl"
#include "../v1/linearDepth.glsl"
#include "../v1/luminance.glsl"

const mat3 from709toDisplayP3 = mat3(    
    0.822461969, 0.033194199, 0.017082631,
    0.1775380,   0.9668058,   0.0723974,
    0.0000000,   0.0000000,   0.9105199
);
const mat3 from709to2020 = mat3(
    vec3(0.6274040, 0.3292820, 0.0433136),
    vec3(0.0690970, 0.9195400, 0.0113612),
    vec3(0.0163916, 0.0880132, 0.8955950)
);
const mat3 fromP3to2020 = mat3(
    vec3(0.753845, 0.198593, 0.047562),
    vec3(0.0457456, 0.941777, 0.0124772),
    vec3(-0.00121055, 0.0176041, 0.983607)
);
const mat3 fromExpanded709to2020  = mat3(
    vec3(0.6274040, 0.3292820, 0.0433136),
    vec3(0.0457456, 0.941777, 0.0124772),
    vec3(-0.00121055, 0.0176041, 0.983607)
);

vec3 tonemap(vec3 c){return c / (vec3(1.0) + c);}

/* Fd is the displayed luminance in cd/m2 */
float PQinverseEOTF(float Fd)
{
    const float Y = Fd / 10000.0;
    const float m1 = pow(Y, 0.1593017578125);
    float res = (0.8359375 + (18.8515625 * m1)) / (1.0 + (18.6875 * m1));
    res = pow(res, 78.84375);
    return res;
}
vec3 PQinverseEOTF(vec3 c)
{
    return vec3(
        PQinverseEOTF(c.x),
        PQinverseEOTF(c.y),
        PQinverseEOTF(c.z));
}

struct S_PPDATA {
    mat4 mtxInvProj;
    vec4 vCameraPos;
    vec4 vFogParams;
    vec4 vSunPos;
    float fExposure;
    float fZnear;
    float fZfar;
    float fHDRLuminance;    
    float fHDRbias;
    bool bHDR;
    int pad0;
    int pad1;
};

layout(location = 0) in vec2 texcoord;

layout(set = 0, binding = 0) uniform sampler2D samp_input;  
layout(set = 0, binding = 1) uniform sampler2D samp_depth;
layout(set = 0, binding = 2) uniform stc_ubo_PostProcessData {
    S_PPDATA ppdata;
};

layout(location = 0) out vec4 fragColor0;

float saturate( float x ){ return clamp( x, 0.0, 1.0 ) ; }
vec3 saturate( vec3 x ){ return clamp( x, 0.0, 1.0 ) ; }

vec3 applyFog(  in float a,
                in float b,        // density falloff
                in vec3  rgb,      // original color of the pixel
                in float d,        // camera to point distance
                in vec3  rayOri,   // camera position
                in vec3  rayDir )  // camera to point vector
{
    vec3 sunDir = normalize(ppdata.vSunPos.xyz);

    float fogAmount = (a/b) * exp( -rayOri.y * b ) * ( 1.0 - exp( -d * rayDir.y * b ) ) / rayDir.y;
    fogAmount = saturate( fogAmount );

    //float fogAmount = 1.0 - exp(-d * b);
    float sunAmount = max( dot(rayDir, sunDir), 0.0 );
    vec3  fogColor  = mix( vec3(0.5,0.6,0.7), // blue
                           vec3(1.0,0.9,0.7), // yellow
                           pow(sunAmount,8.0) );

    return mix( rgb, fogColor, fogAmount );
}

vec3 ComputePositionViewFromZ(vec2 positionScreen, float viewSpaceZ, mat4 mCameraProj)
{
    vec2 screenSpaceRay = vec2( positionScreen.x / mCameraProj[1][1],
                                positionScreen.y / mCameraProj[2][2]);
    
    vec3 positionView;
    positionView.z = viewSpaceZ;
    // Solve the two projection equations
    positionView.xy = screenSpaceRay.xy * positionView.z;
    
    return positionView;
}

float max3(vec3 c) {
    return max(c.r, max(c.g,c.b));
}

vec3 tonemap_and_transfer(in vec3 x)
{
    vec3 r = vec3(0.0);

    if (ppdata.bHDR) {
        // Transer Rec.709 -> Rec.2020 and apply ST.2084 display curve
        r = x * from709to2020;
        r = PQinverseEOTF( r * ppdata.fHDRLuminance );
    } else {
        r = tonemap_filmic( x );
        r = linearTosRGB( x );
    }

    return r;
}

void main() {

    vec4 inColor = texelFetch( samp_input, ivec2(gl_FragCoord.xy), 0 );
    float znorm = texelFetch( samp_depth, ivec2(gl_FragCoord.xy), 0 ).x;
    float linearZ = linearize_depth( znorm, ppdata.fZfar, ppdata.fZnear );
    float fogFactor = 1.0;

    vec4 ndc = vec4(texcoord * 2.0 - 1.0, znorm, 1.0);
    ndc.y = -ndc.y;
    vec4 worldPosition = ppdata.mtxInvProj * ndc;
    worldPosition /= worldPosition.w;

    vec3 cameraToPixel = worldPosition.xyz - ppdata.vCameraPos.xyz;
    float distanceToPixel = length ( cameraToPixel );
    // fogFactor = getFogFactor( ppdata.sFogParams, distanceToPixel );
    

    vec3 fogColor = applyFog(
        ppdata.vFogParams.y,
        ppdata.vFogParams.x,
        inColor.rgb,
        distanceToPixel,
        ppdata.vCameraPos.xyz,
        cameraToPixel / distanceToPixel);
  

    inColor.rgb = mix(inColor.rgb, fogColor, bvec3(ppdata.vFogParams.z > 0.0) );
    inColor.rgb *= ppdata.fExposure;

//    inColor.rgb = ACESFitted( mix( ppdata.sFogParams.color, ppdata.fExposure * inColor.rgb, fogFactor ) );

    inColor.rgb = tonemap_and_transfer(inColor.rgb);
    fragColor0 = vec4( inColor.rgb, inColor.a );
}