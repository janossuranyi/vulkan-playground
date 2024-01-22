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

layout(set = 0, binding = 0) uniform stc_PassDataUBO {
    S_PASS passdata;
};

layout(set = 0, binding = 2) uniform sampler2D samp0;
layout(set = 1, binding = 0) uniform sampler2D samp_albedo;
layout(set = 1, binding = 1) uniform sampler2D samp_normal;
layout(set = 1, binding = 2) uniform sampler2D samp_pbr;


const float PI = 3.14159265359;

// Normal Distribution function --------------------------------------
float D_GGX(float dotNH, float roughness)
{
	float alpha = roughness * roughness;
	float alpha2 = alpha * alpha;
	float denom = dotNH * dotNH * (alpha2 - 1.0) + 1.0;
	return (alpha2)/(PI * denom*denom); 
}

// Geometric Shadowing function --------------------------------------
float G_SchlicksmithGGX(float dotNL, float dotNV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r*r) / 8.0;
	float GL = dotNL / (dotNL * (1.0 - k) + k);
	float GV = dotNV / (dotNV * (1.0 - k) + k);
	return GL * GV;
}

// Fresnel function ----------------------------------------------------
vec3 F_Schlick(vec3 F0, float cosTheta)
{
	vec3 F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0); 
	return F;
}

// Specular BRDF composition --------------------------------------------

vec4 specBRDF(vec3 F0, vec3 L, vec3 V, vec3 N, float roughness)
{
	// Precalculate vectors and dot products	
	vec3 H = normalize (V + L);
	float dotNV = clamp(dot(N, V), 0.0, 1.0);
	float dotNL = clamp(dot(N, L), 0.0, 1.0);
	float dotLH = clamp(dot(L, H), 0.0, 1.0);
	float dotNH = clamp(dot(N, H), 0.0, 1.0);

	vec4 color = vec4(0.0);

    float rroughness = max(0.05, roughness);
    // D = Normal distribution (Distribution of the microfacets)
    float D = D_GGX(dotNH, roughness); 
    // G = Geometric shadowing term (Microfacets shadowing)
    float G = G_SchlicksmithGGX(dotNL, dotNV, rroughness);
    // F = Fresnel factor (Reflectance depending on angle of incidence)
    vec3 F = F_Schlick(F0, dotLH);

    float spec = D * G / (4.0 * dotNL * dotNV + 1e-8); 
    color.rgb = F;
    color.a = spec;

	return color;
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
    vec4 albedoColor = texture(samp_albedo, In.UV);
    vec3 normalTS = texture(samp_normal, In.UV).xyz * 2.0 - 1.0;
    vec4 pbrSample = texture(samp_pbr, In.UV);
    
    // b = 0.5 * sqrt(1 - ( 2 * r - 1)^2 - (2 * g - 1)^2) + 0.5
    //vec3 normalTS = vec3(normalSamp.xy, sqrt(1.0 - normalSamp.x * normalSamp.x - normalSamp.y * normalSamp.y));

    const float r = 0.2 + pbrSample.g * 0.8;
    const float metalness = pbrSample.b;
    const float microAO = pbrSample.r;

//	float roughness = max(perceptualRoughness, step( fract(In.FragCoordVS.y * 2.02), 0.5 ) );

    normalTS = normalize(normalTS) * vec3(1.0,-1.0,1.0);

    vec3 N = (tbn * normalTS);
    vec3 L = normalize(In.LightVS - In.FragCoordVS);
    vec3 V = normalize(-In.FragCoordVS);
    vec3 H = normalize(V + L);
    
    S_LIGHT light;
    light.position = In.LightVS;
    light.direction = In.LightDir;;
    light.innerConeCos = cos(40 * PI/180.0);
    light.outerConeCos = cos(45 * PI/180.0);
    light.range = passdata.vLightPos.w;
    light.color = passdata.vLightColor.rgb;
    light.intensity = passdata.vLightColor.a;
    light.type = LightType_Point;
    vec3 Attn = getLighIntensity( light, In.LightVS - In.FragCoordVS  );

    if(albedoColor.a < 0.5) discard;
    albedoColor.rgb = sRGBToLinear(albedoColor.rgb /* In.Color.rgb*/);

    float NoL = saturate(dot(N,L));
    vec3 F0 = mix(vec3(0.04), albedoColor.rgb, metalness);
    vec3 specColor;
    vec3 diffuseColor;

/*
    const float NoV = saturate(dot(N,V));
    const float NoH = saturate(dot(N,H));
    const float VoH = saturate(dot(V,H));
    
    specColor = GGXSingleScattering(r, F0, NoH, NoV, VoH, NoL);
*/
    vec4 spec = specBRDF(F0, L,V,N, r);
    specColor = spec.rgb * spec.a;

    diffuseColor = (1.0 - metalness) * albedoColor.rgb * (1.0 - spec.rgb);
    
    vec3 ambientColor = 0.02 * albedoColor.rgb;
    float reflectionMask = smoothstep(0.8, 1.0, 1.0 - r);
    out_Color0 = vec4((diffuseColor / PI + specColor.rgb) * Attn * NoL + ambientColor, reflectionMask);
    out_Normal = NormalOctEncode(N,false);
//    outColor0.rgb = mix(checkerColor.rgb, outColor0.rgb, 0.98);
}