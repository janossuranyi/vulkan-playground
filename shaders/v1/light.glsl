#ifndef _LIGHT_INC_GLSL
#define _LIGHT_INC_GLSL

#include "functions.glsl"

const int LightType_Directional = 0;
const int LightType_Point = 1;
const int LightType_Spot = 2;

struct S_LIGHT
{
    vec3 direction;
    float range;

    vec3 color;
    float intensity;

    vec3 position;
    float innerConeCos;

    float outerConeCos;
    float falloff;
    int type;
    int dummy1;
};

/*
watts = blender_lamp.energy (radiant intensity)
LUMENS_PER_WATT = 683
PI = 3.14159
candela = watts * LUMENS_PER_WATT / (4 * PI)
*/


// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#range-property
float getRangeAttenuation(in float range, in float distance, in float falloff)
{
    const float SQR_MIN_DIST = 0.01*0.01;
    const float distanceSqr = max(distance * distance, SQR_MIN_DIST);
    if (range <= 0.0)
    {
        // negative range means unlimited
        return 1.0 / distanceSqr;
    }

    float s = distance / range;

    if (falloff < 0.0)
    {
        s *= s;
        s *= s;
        return max(min(1.0 - s, 1.0), 0.0) / distanceSqr;
    }
    // https://lisyarus.github.io/blog/graphics/2022/07/30/point-light-attenuation.html
    if ( s >= 1.0 ) return 0.0;

    float s2 = sqr( s );
    return sqr(1 - s2) / (1 + falloff * s2);
}

// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#inner-and-outer-cone-angles
float getSpotAttenuation(vec3 pointToLight, vec3 spotDirection, float outerConeCos, float innerConeCos)
{
    float actualCos = dot(normalize(spotDirection), normalize(-pointToLight));
    if (actualCos > outerConeCos)
    {
        if (actualCos < innerConeCos)
        {
            return smoothstep(outerConeCos, innerConeCos, actualCos);
        }
        return 1.0;
    }
    return 0.0;
}

vec3 getLightIntensity(S_LIGHT light, vec3 pointToLight)
{
    float rangeAttenuation = 1.0;
    float spotAttenuation = 1.0;
    float K = 1.0;
    if (light.type != LightType_Directional)
    {
        rangeAttenuation = getRangeAttenuation(light.range, length(pointToLight), light.falloff);
        K = 1.0/(4*M_PI);
    }
    if (light.type == LightType_Spot)
    {
        spotAttenuation = getSpotAttenuation(pointToLight, light.direction, light.outerConeCos, light.innerConeCos);
        K = 1.0/M_PI;
    }

    return rangeAttenuation * spotAttenuation * (light.intensity * K) * light.color;
}


#endif