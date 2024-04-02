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
    int type;
    int dummy0;
    int dummy1;
};

/*
watts = blender_lamp.energy (radiant intensity)
LUMENS_PER_WATT = 683
PI = 3.14159
candela = watts * LUMENS_PER_WATT / (4 * PI)
*/


// https://github.com/KhronosGroup/glTF/blob/master/extensions/2.0/Khronos/KHR_lights_punctual/README.md#range-property
float getRangeAttenuation(float range, float distance)
{
    const float FOUR_PI = 4.0 * 3.14159265359;
    const float LIGHT_SIZE_SQ = 0.01*0.01;
    const float distanceSq = distance * distance;
    if (range <= 0.0)
    {
        // negative range means unlimited
        return 1.0 / max(distanceSq, LIGHT_SIZE_SQ);
    }
    float Kr = distance / range;
    Kr *= Kr;
    Kr *= Kr;
    //return max(min(1.0 - Kr, 1.0), 0.0) / distanceSq;
    return clamp(1.0 - Kr, 0.0, 1.0) / max(distanceSq, LIGHT_SIZE_SQ);
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

    if (light.type != LightType_Directional)
    {
        rangeAttenuation = getRangeAttenuation(light.range, length(pointToLight));
    }
    if (light.type == LightType_Spot)
    {
        spotAttenuation = getSpotAttenuation(pointToLight, light.direction, light.outerConeCos, light.innerConeCos);
    }

    return rangeAttenuation * spotAttenuation * light.intensity * light.color;
}


#endif