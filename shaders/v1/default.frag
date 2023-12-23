#version 450
#extension GL_EXT_nonuniform_qualifier : require
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shader_ballot : require
#extension GL_EXT_shader_explicit_arithmetic_types_float16 : require

//layout(depth_unchanged) out float gl_FragDepth;

//#define USE_BIND 1

#include "material.glsl"
#include "tonemapping.glsl"
#include "colorConversion.glsl"
#include "globals.glsl"
#include "brdf.glsl"
#include "luminance.glsl"
#include "linearDepth.glsl"
#include "fog.glsl"
#include "light.glsl"

layout(location = 0) out vec4 fragColor;

layout(location = 0) in INTERFACE {
	vec4 color;
	vec2 uv;
	vec3 tangent;
	vec3 normal;
	vec3 bitangent;
    vec4 fragPosVS;
    float objectIndex;
} In;


layout(set = 2, binding = 0) readonly buffer g_rbObjects_Layout { S_OBJECT g_rbObjects[]; };
layout(set = 2, binding = 2) readonly buffer g_rbMaterials_Layout { S_MATERIAL g_rbMaterials[]; };
layout(set = 2, binding = 3) uniform stc_UNIFORM_CB_S_LIGHT_BINDING { S_LIGHT g_cbLights[64]; };

vec3 reconstructNormalZ( vec3 n ) {
    return vec3( n.x, n.y, sqrt( 1.0 - saturate( dot ( n.xy, n.xy ) ) ) );
}

struct S_LIGHTING_INPUTS {
    mat3 tbn;
    vec3 normalVS;
    vec4 baseColor;
    vec4 emissiveColor;
    vec3 viewPosVS;
    vec3 fragPosVS;
    vec3 viewDir;
    float perceptualRoughness;
    float alphaRoughness;
    float metallic;
};

#define G_MATERIAL g_rbMaterials[ g_rbObjects[ int(abs(objectIndex)) ].iMaterialIdx ]

vec4 diffuse_pass()
{
    vec4 baseColor;
    vec3 F0;
    vec3 diffuseColor;
    vec3 emissiveColor;
    vec3 normalVS;
    mat3 tbn;
    vec3 finalColor = vec3(0);
    vec3 ambientColor;
    vec3 viewDir = normalize(-In.fragPosVS.xyz);
    float perceptualRoughness;
    float alphaRoughness;
    float metallic;
    float fogFactor = 0.0;
    vec3 fogColor = gvars.vFogColor.rgb;

    const float objectIndex = readFirstInvocationARB(In.objectIndex);
    int baseColorTexture    = G_MATERIAL.baseColorTexture < 0 ? gvars.iWhiteImageIndex : G_MATERIAL.baseColorTexture;
    int normalTexture       = G_MATERIAL.normalTexture < 0 ? gvars.iFlatNormalIndex : G_MATERIAL.normalTexture;
    int specularTexture     = G_MATERIAL.specularTexture < 0 ? gvars.iWhiteImageIndex :  G_MATERIAL.specularTexture;
    int emissiveTexture     = G_MATERIAL.emissiveTexture < 0 ? gvars.iBlackImageIndex : G_MATERIAL.emissiveTexture;

    baseColor = texture(sampler2D(g_2DTextures[ baseColorTexture ], gs_linearRepeat), In.uv);
    baseColor = vec4(sRGBToLinear( baseColor.rgb ) * G_MATERIAL.baseColorFactor.rgb, baseColor.a);
    emissiveColor = texture(sampler2D(g_2DTextures[ emissiveTexture ], gs_linearRepeat), In.uv).rgb;
    emissiveColor *= G_MATERIAL.emissiveFactor.rgb;

    if (baseColor.a < 0.5) {
        discard;
    }

    {
        vec3 normalTS;
        normalTS = texture(sampler2D(g_2DTextures[ normalTexture ], gs_linearRepeat), In.uv).xyz * 2.0 - 1.0;
        normalTS = normalize(normalTS * G_MATERIAL.normalScale.xyz);
        vec3 t = normalize(In.tangent);
        vec3 b = normalize(In.bitangent);
        vec3 n = normalize(In.normal);
        tbn = mat3(t,b,n);
        normalVS = (tbn * normalTS);
    }
    {
        vec4 mr;
        mr = texture(sampler2D(g_2DTextures[ specularTexture ], gs_linearRepeat), In.uv);
        perceptualRoughness = mr.g;
        alphaRoughness = perceptualRoughness * perceptualRoughness;
        metallic = mr.b;
    }

    diffuseColor = (1.0 - metallic) * baseColor.rgb;
    F0 = mix(vec3(0.04), baseColor.rgb, metallic);
    ambientColor = baseColor.rgb * gvars.vAmbientColor.rgb * gvars.vAmbientColor.a;

    const vec3 fr = diffuseColor / pi;
    for(int light = 0; light < g_cbLights.length(); ++light)
    {
        const vec4 lightPosVS = gvars.mView * vec4(g_cbLights[light].position,1.0);
        const vec3 lightDir = normalize(lightPosVS.xyz - In.fragPosVS.xyz);
        const float NoL = saturate(dot(normalVS, lightDir));
        const vec3 directLight = getLighIntensity(g_cbLights[light], lightPosVS.xyz - In.fragPosVS.xyz) * NoL;
        
        if (computeLuminance(directLight) < 5.0/255.0) {
            continue;
        }

        vec3 specColor = specBRDF(normalVS, viewDir, lightDir, F0, perceptualRoughness);
        finalColor += directLight * (diffuseColor + specColor);
    }

    //FOG
    {
        //float fogCoordinate = linearize_depth(gl_FragCoord.z, gvars.fNearPlane, gvars.fFarPlane);
        float fogCoordinate = abs(In.fragPosVS.z);
        FogParameters fparms = FogParameters(gvars.vFogColor.rgb,0.0,0.0,gvars.fFogDensity,1,true);
        fogFactor = getFogFactor(fparms,fogCoordinate);
    }

    finalColor += ambientColor;
    finalColor += emissiveColor;
    finalColor *= gvars.fExposureBias;
    finalColor = mix(fogColor, finalColor, fogFactor);
    finalColor = finalColor / (finalColor + 1.0);

    return vec4( linearTosRGB( finalColor /* whiteScale*/ ), baseColor.a );
}

vec4 forward_pass()
{
    const float objectIndex = readFirstInvocationARB(In.objectIndex);

    S_LIGHTING_INPUTS inputs = S_LIGHTING_INPUTS(mat3(0),vec3(0),vec4(1),vec4(0),vec3(0),vec3(0),vec3(0),1.0,1.0,0.0);
    const vec3 whiteScale = 1.0/tonemap_Uncharted2(vec3(11.2));

    int baseColorTexture    = G_MATERIAL.baseColorTexture < 0 ? gvars.iWhiteImageIndex : G_MATERIAL.baseColorTexture;
    int specularTexture     = G_MATERIAL.specularTexture < 0 ? gvars.iWhiteImageIndex :  G_MATERIAL.specularTexture;
    int normalTexture       = G_MATERIAL.normalTexture < 0 ? gvars.iFlatNormalIndex : G_MATERIAL.normalTexture;
    int emissiveTexture     = G_MATERIAL.emissiveTexture < 0 ? gvars.iBlackImageIndex : G_MATERIAL.emissiveTexture;
    
    {
        vec4 baseColor = texture(sampler2D(g_2DTextures[ baseColorTexture ], gs_linearRepeat), In.uv);
        vec4 emissiveColor = texture(sampler2D(g_2DTextures[ emissiveTexture ], gs_linearRepeat), In.uv);
        inputs.baseColor  = vec4(sRGBToLinear( baseColor.rgb ), baseColor.a);
        inputs.emissiveColor = vec4(sRGBToLinear( emissiveColor.rgb ), 1.0) * G_MATERIAL.emissiveFactor;
        inputs.baseColor *= In.color * G_MATERIAL.baseColorFactor;
    }

    if (inputs.baseColor.a < 0.5) {
        discard;
    }

    {
        vec2 spec = texture(sampler2D(g_2DTextures[ specularTexture ], gs_linearRepeat), In.uv).gb;
        inputs.perceptualRoughness = clamp(spec.x, 0.0, 1.0);
        spec.x = 0.2 + spec.x * 0.8;
        inputs.alphaRoughness = spec.x * spec.x;
        inputs.metallic = spec.y;
    }

    {
        vec3 normalTS = texture(sampler2D(g_2DTextures[ normalTexture ], gs_linearRepeat), In.uv).xyz * 2.0 - 1.0;
        normalTS = reconstructNormalZ(normalTS);
        normalTS = normalize(normalTS * G_MATERIAL.normalScale.xyz);
        vec3 t = normalize(In.tangent);
        vec3 b = normalize(In.bitangent);
        vec3 n = normalize(In.normal);
        inputs.tbn = mat3(t,b,n);
        inputs.normalVS = (inputs.tbn * normalTS);

        if(any(isnan(inputs.normalVS))){
            inputs.normalVS = In.normal; //fix for broken (bi)tangents on tree assets
        }

    }

    const vec3 fogColor = gvars.vFogColor.rgb;
    float fogFactor = 0.0;

    //FOG
    {
        //float fogCoordinate = linearize_depth(gl_FragCoord.z, gvars.fNearPlane, gvars.fFarPlane);
        float fogCoordinate = abs(In.fragPosVS.z);
        FogParameters fparms = FogParameters(fogColor,0.0,0.0,gvars.fFogDensity,1,true);
        fogFactor = getFogFactor(fparms,fogCoordinate);
    }

    inputs.viewDir = normalize(-In.fragPosVS.xyz);
    vec3 diffuse = (1.0 - inputs.metallic) * inputs.baseColor.rgb;
    vec3 ambient = diffuse * gvars.vAmbientColor.rgb * gvars.vAmbientColor.a;
    const vec3 N = inputs.normalVS;
    const vec3 V = inputs.viewDir;
    const float NdotV = max(saturate(dot(N, V)),0.0001); 
    const vec3 f0 = mix( vec3(0.04), inputs.baseColor.rgb, inputs.metallic );

#define GET_LIGHT(x) (g_cbLights[x])

    vec3 finalColor = vec3(0);
    for(int light = 0; light < g_cbLights.length(); ++light)
    {
        vec3 L;
        const vec4 lightPosVS = gvars.mView * vec4(GET_LIGHT(light).position,1.0);
        if (GET_LIGHT(light).type == LightType_Directional || GET_LIGHT(light).type == LightType_Spot)
        {
            L = GET_LIGHT(light).direction;
        } else {
            L = normalize( lightPosVS.xyz - In.fragPosVS.xyz );
        }

        vec3 directLight = getLighIntensity(GET_LIGHT(light), lightPosVS.xyz - In.fragPosVS.xyz);
        if (computeLuminance(directLight) < 5.0/255.0) {
            continue;
        }
        const float NdotL = saturate(dot(N, L));
        const vec3 H = normalize(V + L);
        const float NdotH = saturate(dot(N, H));
        const float VdotH = saturate(dot(V, H));
        directLight *= NdotL;

//        const vec3 specularDirect = directLight * specBRDF( NdotH, NdotV, NdotL, VdotH, f0, inputs.alphaRoughness );
        const vec3 specularDirect = directLight * GGXSingleScattering(inputs.alphaRoughness, f0, NdotH, NdotV, VdotH, NdotL);
        const vec3 fr = CoDWWIIDiffuse(diffuse, NdotL, VdotH, NdotV, NdotH, inputs.alphaRoughness);
//        const vec3 fr = diffuse / pi;
        vec3 diffuseDirect = directLight * fr;
        //need to account for incoming and outgoing fresnel effect
        //see: https://seblagarde.wordpress.com/2011/08/17/hello-world/#comment-2405
        diffuseDirect *= (1.0 - fresnelSchlick(f0, NdotV)) * (1.0 - (fresnelSchlick(f0, NdotL)));
        finalColor += specularDirect + diffuseDirect;
    }    
#undef GET_LIGHT
    finalColor = mix(fogColor, inputs.emissiveColor.rgb + ((ambient + finalColor) * gvars.fExposureBias), fogFactor);
    //finalColor = tonemap_Uncharted2(finalColor);
    finalColor = finalColor / (1.0 + finalColor);
    
    return vec4( linearTosRGB( finalColor /* whiteScale*/ ), 1.0 );
}

void main() {
//    fragColor = forward_pass();
    fragColor = diffuse_pass();
}
