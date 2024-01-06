#ifndef BRDF_INC
#define BRDF_INC

#include "functions.glsl"

float D_GGX(const float NoH, const float r){
    float a = NoH * r;
    float k = r / (1.0 - NoH * NoH + a * a);
    return k * k * (1.0 / pi);
}

//geometric occlusion function
float G_GGX(const float r, const float XoN){
    const float r_2 = r * r;
    const float nom = XoN * 2.f;
    const float denom = XoN + sqrt(r_2 + (1.f - r_2) * XoN * XoN);
    return nom / denom;
}

//visibility function
//combines G with denominator
//uses height correlated Smith
float Visibility(const float NoV, const float NoL, const float r){
    const float r_2 = r * r;
    const float v1 = NoL * sqrt(NoV * NoV * (1.f - r_2) + r_2);
    const float v2 = NoV * sqrt(NoL * NoL * (1.f - r_2) + r_2);
    return 0.5f / (v1 + v2 + 1e-8);
}

//combination of viewer occlusion and facet shadowing
//uncorrelated smith
float G_Smith(const float r, const float NoV, const float NoL){
    return G_GGX(r, NoV) * G_GGX(r, NoL);
}

vec3 fresnelSchlick ( vec3 f0, float costheta ) {
	//const float baked_spec_occl = saturate( 50.0 * dot( f0, vec3( 0.3333 ) ) );
	return f0 + ( 1.0 - f0 ) * ApproxPow( saturate( 1.0 - costheta ), 5.0 );
}

vec3 F_Schlick(vec3 f0, vec3 f90, float VoH){
    return f0 + (f90 - f0) * pow(1.f - VoH, 5.f);
}

//uses energy conservation from frostbite PBR paper
vec3 DisneyDiffuse(vec3 diffuseColor, float NoL, float VoH, float NoV, float r){

    float fresnelDiffuse90 = 0.5f + 2.f * VoH * VoH * r;        
    float energyBias = mix(0.f, 0.5f, r);
    float energyFactor = mix(1.f, 1.f / 1.51f, r);
    float fresnelDiffuse90Biased = energyBias + 2.f * VoH * VoH * r;
    return diffuseColor / pi * F_Schlick(vec3(1.f), vec3(fresnelDiffuse90Biased), NoL) * F_Schlick(vec3(1.f), vec3(fresnelDiffuse90Biased), NoV) * energyFactor;
}

//conversion from roughness to gloss computed from papers gloss to roughness formula
vec3 CoDWWIIDiffuse(vec3 diffuseColor, float NoL, float VoH, float NoV, float NoH, float r){
    float f0Diffuse = VoH + pow(1.f - VoH, 5.f);
    float f1 =  (1.f - 0.75f * pow(1.f - NoL, 5.f)) *
                (1.f - 0.75f * pow(1.f - NoV, 5.f));
    float g = log2(2.f / (r * r) - 1.f) / 18.f;
    float t = clamp(2.2f * g - 0.5f, 0.f, 1.f);
    float fd = f0Diffuse + (f1 - f0Diffuse) * t;
    float fb = (34.5f * g * g - 59.f * g + 24.5f) * VoH * pow(2.f, -max(73.2f * g - 21.2f, 8.9f) * sqrt(NoH));
    return diffuseColor / pi * (fd + fb);
}

float Titanfall2DiffuseSingleComponent(float NoL, float LoV, float NoV, float NoH, float r){
    float facing = 0.5f + 0.5f * LoV;
    float rough = facing * (0.9f - 0.4f * facing) * (0.5f + NoH) / max(NoH, 0.03f);
    float smoothDiffuse = 1.05f *   (1.f - pow(1.f - NoL, 5.f)) *
                                    (1.f - pow(1.f - NoV, 5.f));
    return 1.f / pi * mix(smoothDiffuse, rough, r);
}

vec3 Titanfall2Diffuse(vec3 diffuseColor, float NoL, float LoV, float NoV, float NoH, float r){
    float single = Titanfall2DiffuseSingleComponent(NoL, LoV, NoV, NoH, r);
    float multi = 0.1159f * r;
    return diffuseColor * (single + diffuseColor * multi);
}

vec3 GGXSingleScattering(float r, vec3 f0, float NoH, float NoV, float VoH, float NoL){
    const float D = D_GGX(NoH, r);
    const float Vis = Visibility(NoV, NoL, r);
    const vec3 F = F_Schlick(f0, vec3(1.f), VoH);
    return D * Vis * F;
}

/**************************************/
/* Cook-Torrance specular BRDF.       */
/* Based on DOOM 2016 lighting shader */
/**************************************/
vec3 specBRDF( vec3 N, vec3 V, vec3 L, vec3 f0, float r ) {
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
	return fresnelSchlick( f0, clamp(dot( L, H ), 0.0, 1.0) ) * spec;
}

/* Modified DOOM2016 BRDF */
vec3 specBRDF(float NoH, float NoV, float NoL, float VoH, vec3 f0, float a)
{
    float a2 = a * a;
    float spec = (NoH * NoH) * (a2 - 1) + 1;
    spec = a2 / ( spec * spec + 1e-8 );
	float Gv = NoV * (1.0 - a) + a;
	float Gl = NoL * (1.0 - a) + a;
    spec /= ( 4.0 * Gv * Gl + 1e-8 );
    return F_Schlick( f0, vec3(1.0), VoH ) * spec;
}
#endif // #ifndef BRDF_INC