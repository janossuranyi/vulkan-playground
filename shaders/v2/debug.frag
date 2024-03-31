#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) out vec4 outColor0;
layout(location = 0) in vec3 color0;

const mat3 from709toDisplayP3 = mat3(    
    vec3(0.822461969, 0.033194199, 0.017082631),
    vec3(0.1775380,   0.9668058,   0.0723974),
    vec3(0.0000000,   0.0000000,   0.9105199)
);
const mat3 from709to2020 = mat3(
    vec3(0.6274040, 0.3292820, 0.0433136),
    vec3(0.0690970, 0.9195400, 0.0113612),
    vec3(0.0163916, 0.0880132, 0.8955950)
);

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

#include "../v1/tonemapping.glsl"

const float gamma = 1.0/2.2;

layout(push_constant) uniform pc_data {
    vec4 data;
} pc;

#define Exposure (pc.data.y)

const bool HDR = false;
//layout(set = 0, binding = 1) uniform sampler2D images[16];
void main()
{
    const float DisplayMaxLuminance = pc.data.z;
    const float WhiteLevel = pc.data.w;

    vec3 c = color0 * pc.data.xxx;
    c = tonemap_filmic(c * Exposure);
    if (HDR)
    {
        c *= from709to2020;
        c *= (DisplayMaxLuminance/10000.0);
        //c += (WhiteLevel / 10000.0);
        c = ApplyPQ( c ) ;
    }
    else
    {
        c = pow(c, vec3(gamma));
    }
    outColor0 = vec4(c, 1.0);
}