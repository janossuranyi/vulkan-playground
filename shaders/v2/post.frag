#version 450
#extension GL_GOOGLE_include_directive : require

#include "../v1/tonemapping.glsl"
#include "../v1/colorConversion.glsl"
#include "../v1/fog.glsl"
#include "../v1/linearDepth.glsl"
#include "../v1/luminance.glsl"


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

vec3 ApplyPQ(vec3 color)
{
    // Apply ST2084 curve
    const float m1 = 2610.0 / 4096.0 / 4;
    const float m2 = 2523.0 / 4096.0 * 128;
    const float c1 = 3424.0 / 4096.0;
    const float c2 = 2413.0 / 4096.0 * 32;
    const float c3 = 2392.0 / 4096.0 * 32;
    const vec3 cp = pow(abs(color.xyz), vec3(m1));
    color.xyz = pow((c1 + c2 * cp) / (1 + c3 * cp), vec3(m2));
    return color;
}

float InversePQ_approx(float x)
{
    float k = pow((x * 0.01f), 0.1593017578125);
    return (3.61972*(1e-8) + k * (0.00102859 + k * (-0.101284 + 2.05784 * k))) / (0.0495245 + k * (0.135214 + k * (0.772669 + k)));
}

struct S_PPDATA {
    mat4 mtxInvProj;
    vec4 vCameraPos;
    vec4 vFogParams;
    vec4 vSunPos;
    float fExposure;
    float fZnear;
    float fZfar;
    float tonemapper_P;  // Max brightness.
    float tonemapper_a;    // contrast
    float tonemapper_m;   // linear section start
    float tonemapper_l;    // linear section length
    float tonemapper_c;   // black
    float tonemapper_b;    // pedestal
    float tonemapper_s;  // scale 
    float saturation;
    bool bHDR;
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

vec3 applySaturationSRGB(vec3 c, float saturation)
{
    return mix(vec3(computeLuminance(c)), c, saturation);
}

vec3 tonemap_and_transfer(in vec3 x)
{
    //vec3 r = tonemap_Uncharted2( x );

    const float P = ppdata.bHDR ? ppdata.tonemapper_P : min(1.0, ppdata.tonemapper_P); // max display brightness.
    const float a = ppdata.tonemapper_a;  // contrast
    const float m = ppdata.tonemapper_m; // linear section start
    const float l = ppdata.tonemapper_l;  // linear section length
    const float c = ppdata.tonemapper_c; // black
    const float b = ppdata.tonemapper_b;  // pedestal
    vec3 r;
    // Tonemapper in srgb color space.
    //r = uchimuraTonemapper(x, P, a, m, l, c, b);
    r = ACESFilmApproximate( x );
    // apply saturation in srgb space.
    r = applySaturationSRGB(r, ppdata.saturation);

    if (ppdata.bHDR) {
        // Transer Rec.709 -> Rec.2020 and apply ST.2084 display curve
        r = sRGB_2_Rec2020 * r;
        r *= ppdata.tonemapper_s * (100.0 / 10000.0);
        r = ApplyPQ( r );
    }
    else
    {
        r = linearTosRGB( r );
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