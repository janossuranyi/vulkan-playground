#version 450
#extension GL_GOOGLE_include_directive : require

#ifndef pi
const float pi = 3.1415926535;
#endif
#include "../v1/functions.glsl"
#include "../v1/brdf.glsl"
#include "../v1/colorConversion.glsl"
#include "../v1/tonemapping.glsl"
#include "../v1/light.glsl"
#include "../v1/fog.glsl"

layout(location = 0) out vec4 outColor0;
layout(location = 1) out vec2 outNormal;

layout(location = 0) in INTERFACE {
    vec4 Color;
    vec3 FragCoordVS;
    vec3 LightVS;
    vec3 LightDir;
    vec2 UV;
    vec3 NormalVS;
    vec3 TangentVS;
    vec3 BitangentVS;
} In;


layout(set = 0, binding = 2) uniform sampler2D samp0;

layout(set = 1, binding = 0) uniform sampler2D samp_albedo;
layout(set = 1, binding = 1) uniform sampler2D samp_normal;
layout(set = 1, binding = 2) uniform sampler2D samp_pbr;

void main() {

    FogParameters fogParams;
    fogParams.color = vec3(0.75);
    fogParams.equation = 1;
    fogParams.isEnabled = true;
    fogParams.density = 0.01;

    float fogFactor = getFogFactor(fogParams, abs(In.FragCoordVS.z));

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

    const float perceptualRoughness = pbrSample.g;
    const float r = max(perceptualRoughness * perceptualRoughness, 0.0045f);
    const float metalness = pbrSample.b;
    const float microAO = pbrSample.r;
    const float Kd = (1.0 - metalness);

    normalTS = normalize(normalTS) * vec3(1.0,-1.0,1.0);

    vec3 N = (tbn * normalTS);
    vec3 L = normalize(In.LightVS - In.FragCoordVS);
    vec3 V = normalize(-In.FragCoordVS);
    vec3 H = normalize(V + L);
    
    S_LIGHT light;
    light.position = In.LightVS;
    light.direction = In.LightDir;;
    light.innerConeCos = cos(40 * pi/180.0);
    light.outerConeCos = cos(45 * pi/180.0);
    light.range = 20.0;
    light.color = vec3(1.0);
    light.intensity = 300.0;
    light.type = LightType_Point;
    vec3 Attn = getLighIntensity( light, In.LightVS - In.FragCoordVS  );

    if(albedoColor.a < 0.5) discard;
    albedoColor.rgb = sRGBToLinear(albedoColor.rgb);
    vec3 diffuseColor = albedoColor.rgb * Kd;

    float NoL = saturate(dot(N,L));
    vec3 F0 = mix(vec3(0.04), albedoColor.rgb, metalness);
    vec3 specColor;
/*
    float NoH = saturate(dot(N,H));
    float NoV = saturate(dot(N,V));
    float VoH = saturate(dot(V,H));
    
    specColor = GGXSingleScattering(r, F0, NoH, NoV, VoH, NoL);
*/
//    specColor = specBRDF(NoH,NoV,NoL,VoH,F0,r);

    specColor = specBRDF(N,V,L,F0,perceptualRoughness);
    diffuseColor /= pi; //CoDWWIIDiffuse(diffuseColor, NoL, VoH, NoV, NoH, r);
    
/*
    vec3 ddx = dFdx( In.FragCoordVS );
    vec3 ddy = dFdy( In.FragCoordVS );
    vec3 N = normalize( cross( ddx, ddy ) );
*/
    vec3 ambientColor = 0.05 * albedoColor.rgb;
    outColor0 = vec4((diffuseColor + specColor) * Attn * NoL + ambientColor, 1.0 - r);
    outColor0.rgb = mix(fogParams.color, outColor0.rgb, fogFactor);
    outNormal = NormalOctEncode(N,false);
//    outColor0.rgb = mix(checkerColor.rgb, outColor0.rgb, 0.98);
}