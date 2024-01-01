#version 450
#extension GL_GOOGLE_include_directive : require

#ifndef pi
const float pi = 3.1415926535;
#endif
#include "../v1/brdf.glsl"
#include "../v1/colorConversion.glsl"
#include "../v1/tonemapping.glsl"

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


void main() {

    mat3 tbn = mat3(
        normalize(In.TangentVS),
        normalize(In.BitangentVS),
        normalize(In.NormalVS));

    vec4 checkerColor = texture(samp0,In.UV);
    vec4 albedoColor = texture(samp_albedo, In.UV);
    vec3 normalTS = texture(samp_normal, In.UV).xyz * 2.0 - 1.0;
    vec4 pbrSample = texture(samp_pbr, In.UV);

    const float perceptualRoughness = pbrSample.g;
    const float alphaRoughness = perceptualRoughness * perceptualRoughness;
    const float metalness = pbrSample.b;

    normalTS = normalize(normalTS * vec3(1.0,-1.0,1.0));

    vec3 N = (tbn * normalTS);
    vec3 L = normalize(In.LightVS);
    vec3 V = normalize(-In.FragCoordVS);
    //vec3 H = normalize(L + V);
    
    albedoColor.rgb = sRGBToLinear(albedoColor.rgb);

    if(albedoColor.a < 0.5) discard;

    vec3 diffuseColor = (1.0 - metalness) * albedoColor.rgb;
    vec3 F0 = mix(vec3(0.04), albedoColor.rgb, metalness);
    vec3 specColor = specBRDF(N, V, L, F0, perceptualRoughness);

/*
    vec3 ddx = dFdx( In.FragCoordVS );
    vec3 ddy = dFdy( In.FragCoordVS );
    vec3 N = normalize( cross( ddx, ddy ) );
*/
    float intensity = max(dot(N, L),0.0);
    vec3 lightColor = vec3(15,15,15) * vec3(intensity);

    vec3 ambient = 0.05 * albedoColor.rgb;
    const vec3 fr = diffuseColor / pi;
    outColor0 = vec4((diffuseColor + specColor) * lightColor + ambient, albedoColor.a);
    //outColor0 = vec4(vec3(spec), albedoColor.a);
    outColor0.rgb = tonemap_Uncharted2( outColor0.rgb );
}