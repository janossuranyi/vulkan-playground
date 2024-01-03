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

layout(location = 0) out vec4 outColor0;

layout(location = 0) in INTERFACE {
    vec4 Color;
    vec3 FragCoordVS;
    vec3 LightVS;
    vec2 UV;
    vec3 NormalVS;
    vec3 TangentVS;
    vec3 BitangentVS;
} In;


layout(set = 0, binding = 2) uniform sampler2D samp0;

layout(set = 1, binding = 0) uniform sampler2D samp_albedo;
layout(set = 1, binding = 1) uniform sampler2D samp_normal;
layout(set = 1, binding = 2) uniform sampler2D samp_pbr;

vec3 toSRGB(vec3 color) 
{
    return pow(color, vec3(1.0/2.2));
}

vec3 tonemap(vec3 c) { return c / (1.0 + c); }

void main() {

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
    const float alphaRoughness = max(perceptualRoughness * perceptualRoughness, 0.0045f);
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
    light.range = 10.0;
    light.color = vec3(1.0);
    light.intensity = 35.0;
    light.type = LightType_Point;
    vec3 Attn = getLighIntensity(light, In.LightVS - In.FragCoordVS);

    if(albedoColor.a < 0.5) discard;
    albedoColor.rgb = sRGBToLinear(albedoColor.rgb);
    vec3 diffuseColor = albedoColor.rgb * Kd;

    float NoL = saturate(dot(N,L));
    vec3 F0 = mix(vec3(0.04), albedoColor.rgb, metalness);

    float NoH = saturate(dot(N,H));
    float NoV = saturate(dot(N,V));
    float VoH = saturate(dot(V,H));
    vec3 specColor;
    specColor = GGXSingleScattering(max(alphaRoughness,0.2), F0, NoH, NoV, VoH, NoL);
    diffuseColor = CoDWWIIDiffuse(diffuseColor, NoL, VoH, NoV, NoH, alphaRoughness);
    
/*
    vec3 ddx = dFdx( In.FragCoordVS );
    vec3 ddy = dFdy( In.FragCoordVS );
    vec3 N = normalize( cross( ddx, ddy ) );
*/
    vec3 ambient = 0.02 * albedoColor.rgb;
    outColor0 = vec4((diffuseColor + specColor) * Attn * NoL + ambient, albedoColor.a);
    //outColor0 = vec4(vec3(spec), albedoColor.a);
    outColor0.rgb = toSRGB( ACESFilmApproximate ( outColor0.rgb ) );
}