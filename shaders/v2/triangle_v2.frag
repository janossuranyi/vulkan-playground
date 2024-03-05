#version 450
#extension GL_GOOGLE_include_directive : require

#include "../v1/functions.glsl"
//#include "../v1/brdf.glsl"
#include "../v1/colorConversion.glsl"
#include "../v1/tonemapping.glsl"
#include "../v1/light.glsl"

#include "passData.glsl"

layout(location = 0) out vec4 out_Color0;
layout(location = 1) out vec2 out_Normal;

struct S_INTERFACE {
    vec4 Color;
    vec3 FragCoordVS;
    vec3 LightVS;
    vec3 LightDir;
    vec2 UV;
    vec3 NormalVS;
    vec3 TangentVS;
    vec3 BitangentVS;
};

layout(location = 0) in INTERFACE {
    S_INTERFACE In;
};

layout(set = 0, binding = 0) uniform stc_ubo_PassData {
    S_PASS passdata;
};

layout(set = 0, binding = 3) uniform stc_ubo_LightData {
    S_LIGHT lightdata[16];
};

layout(set = 0, binding = 2) uniform sampler2D samp0;
layout(set = 1, binding = 0) uniform sampler2D samp_material[4];

#define SAMP_ALBEDO 0
#define SAMP_NORMAL 1
#define SAMP_PBR 2
#define SAMP_EMISSIVE 3

/*
layout(set = 1, binding = 0) uniform sampler2D samp_albedo;
layout(set = 1, binding = 1) uniform sampler2D samp_normal;
layout(set = 1, binding = 2) uniform sampler2D samp_pbr;
*/

const float PI = 3.14159265359;

// Normal Distribution function --------------------------------------
float D_GGX(float NoH, float a) {
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * f * f);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a) {
    float a2 = a * a;
    float GGXL = NoV * sqrt((-NoL * a2 + NoL) * NoL + a2);
    float GGXV = NoL * sqrt((-NoV * a2 + NoV) * NoV + a2);
    return 0.5 / (GGXV + GGXL + 1e-8);
}

float V_SmithGGXCorrelatedFast(float NoV, float NoL, float roughness) {
    float a = roughness;
    float GGXV = NoL * (NoV * (1.0 - a) + a);
    float GGXL = NoV * (NoL * (1.0 - a) + a);
    return 0.5 / (GGXV + GGXL + 1e-8);
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(vec3 F0, float cosTheta)
{
	vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); 
	return F;
}

float Fd_Lambert() {
    return 1.0 / PI;
}

// Specular BRDF composition --------------------------------------------
vec3 specBRDF(vec3 f0, vec3 l, vec3 v, vec3 n, float perceptualRoughness)
{
    vec3 h = normalize(v + l);

    float NoV = abs(dot(n, v)) + 1e-5;
    float NoL = clamp(dot(n, l), 0.0, 1.0);
    float NoH = clamp(dot(n, h), 0.0, 1.0);
    float LoH = clamp(dot(l, h), 0.0, 1.0);

    // perceptually linear roughness to roughness (see parameterization)
    float roughness = perceptualRoughness;
    roughness *= roughness;
    roughness = clamp(roughness, 0.089, 1.0);
    
    float D = D_GGX(NoH, roughness);
    vec3  F = F_Schlick(f0, LoH);
    float V = V_SmithGGXCorrelatedFast(NoV, NoL, roughness);

    // specular BRDF
    vec3 Fr = (D * V) * F;
    
    return Fr;
}


vec3 specBRDF_DOOM( vec3 f0,vec3 L, vec3 V, vec3 N, float r ) {
	const vec3 H = normalize( V + L );
	float m = 0.2 + r * 0.8;
    m *= m;
    m *= m;
    float m2 = m * m;
	float NdotH = clamp( dot( N, H ), 0.0, 1.0 );
	float spec = (NdotH * NdotH) * (m2 - 1) + 1;
	spec = m2 / ( spec * spec + 1e-8 );
	float Gv = clamp( dot( N, V ), 0.0, 1.0 ) * (1.0 - m) + m;
	float Gl = clamp( dot( N, L ), 0.0, 1.0 ) * (1.0 - m) + m;
	spec /= ( 4.0 * Gv * Gl + 1e-8 );
    
	return F_Schlick( f0, clamp(dot( L, H ), 0.0, 1.0) ) * spec;
}


void main() {

/*
    vec3 ddx = dFdx( In.FragCoordVS );
    vec3 ddy = dFdy( In.FragCoordVS );
    vec3 N = normalize( cross( ddx, ddy ) );
*/

    mat3 tbn = mat3(
        normalize(In.TangentVS),
        normalize(In.BitangentVS),
        normalize(In.NormalVS));

    vec4 checkerColor = texture(samp0,In.UV);
    vec4 albedoColor = texture(samp_material[SAMP_ALBEDO], In.UV);
    vec3 normalTS = texture(samp_material[SAMP_NORMAL], In.UV).xyz * 2.0 - 1.0;
    vec4 pbrSample = texture(samp_material[SAMP_PBR], In.UV);
    vec3 emissiveColor = texture(samp_material[SAMP_EMISSIVE], In.UV).rgb;

    const bool hasEmissive = any(greaterThan(emissiveColor, vec3(0)));

    if (albedoColor.a < .5) discard;

    albedoColor.rgb = sRGBToLinear(albedoColor.rgb /* In.Color.rgb*/);

    if (hasEmissive) {
        out_Color0 = vec4(emissiveColor,albedoColor.a);
        return;
    }

    //normalTS.z = sqrt(1.0 - dot(normalTS.xy, normalTS.xy));

//  const float r = 0.2 + pbrSample.g * 0.8;
    const float r = pbrSample.g;
    const float metalness = pbrSample.b;
    const float microAO = pbrSample.r;
    const float reflectance = 0.5;
    const float Df0 = 0.16 * reflectance * reflectance;
    const vec3 diffuseColor = (1.0 - metalness) * albedoColor.rgb;

//	float roughness = max(perceptualRoughness, step( fract(In.FragCoordVS.y * 2.02), 0.5 ) );

    normalTS = normalize(normalTS) * vec3(1.0,-1.0,1.0);
    vec3 F0 = mix(vec3(Df0), albedoColor.rgb, metalness);

    vec3 N = (tbn * normalTS);
    vec3 V = normalize(-In.FragCoordVS);
/*    
    S_LIGHT light;
    light.position = In.LightVS;
    light.direction = In.LightDir;;
    light.innerConeCos = cos(40 * PI/180.0);
    light.outerConeCos = cos(45 * PI/180.0);
    light.range = passdata.vLightPos.w;
    light.color = passdata.vLightColor.rgb;
    light.intensity = passdata.vLightColor.a;
    light.type = LightType_Point;
*/
    const vec3 Fd = diffuseColor * Fd_Lambert();
    vec3 finalColor = vec3(0.0);
    for(uint i = 0; i < lightdata.length(); ++i)
    {
        vec4 lightPosVS = passdata.mtxView * vec4(lightdata[i].position,1.0);
        vec3 L = normalize(lightPosVS.xyz - In.FragCoordVS);
        vec3 H = normalize(V + L);
        float NoL = saturate(dot(N,L));

        vec3 lightColor = getLightIntensity( lightdata[i], lightPosVS.xyz - In.FragCoordVS  );

        vec3 Fr = specBRDF(F0, L,V,N, r);
        finalColor += (Fr + Fd) * lightColor * NoL;
    }

    vec3 ambientColor = passdata.vParams.x * albedoColor.rgb;
    float reflectionMask = smoothstep(0.8, 1.0, 1.0 - r);
    out_Color0 = vec4(finalColor + ambientColor, reflectionMask);
    out_Normal = NormalOctEncode(N,false);
//    outColor0.rgb = mix(checkerColor.rgb, outColor0.rgb, 0.98);
}