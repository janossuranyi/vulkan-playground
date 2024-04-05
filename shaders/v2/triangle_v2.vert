#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;
//layout(location = 4) in vec4 inColor;

#include "passData.glsl"

layout(set = 0, binding = 0) uniform stc_ubo_PassData {
    S_PASS passdata;
};

struct S_DRAW_DATA {
    mat4 mtxModel;
    mat4 mtxNormal;
    vec4 color;
};

layout(set = 0, binding = 1) uniform dyn_ubo_DrawData {
    S_DRAW_DATA drawdata;
};

struct S_INTERFACE {
    vec3 FragCoordVS;
    vec2 UV;
    vec3 NormalVS;
    vec3 TangentVS;
    vec3 BitangentVS;
};

layout(location = 0) out INTERFACE {
    S_INTERFACE Out;
};

void main() {
    vec3 light = passdata.vLightPos.xyz;
    vec4 posVS = ( passdata.mtxView * drawdata.mtxModel ) * vec4( inPosition, 1.0 );
    
    //output the position of each vertex
    gl_Position = passdata.mtxProjection * posVS;
    gl_Position.y = -gl_Position.y;

	mat3 mtxNormal = mat3( passdata.mtxView * drawdata.mtxNormal );
	
	vec4 localTangent = inTangent * 2.0 - 1.0;
	vec3 localNormal = inNormal * 2.0 - 1.0;

	vec3 T = normalize( mtxNormal * localTangent.xyz );
	vec3 N = normalize( mtxNormal * localNormal );
	T = normalize(T - dot( T, N ) * N);
	vec3 B = normalize( cross( N, T ) * localTangent.w );

    Out.NormalVS = N;
    Out.TangentVS = T;
    Out.BitangentVS = B;
    Out.FragCoordVS = posVS.xyz;
    Out.UV = inUV;
}
