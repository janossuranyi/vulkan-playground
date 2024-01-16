#version 450
#extension GL_GOOGLE_include_directive : require
#extension GL_ARB_shader_draw_parameters : require

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inColor;

#include "passData.glsl"

layout(set = 0, binding = 0) uniform PassData_ubo {
    S_PASS passdata;
};

struct S_DRAW_DATA {
    mat4 mtxModel;
    mat4 mtxNormal;
    vec4 color;
};

layout(set = 0, binding = 1) uniform DrawData_ubo {
    S_DRAW_DATA drawdata[256];
};

layout(location = 0) out INTERFACE {
    vec4 Color;
    vec3 FragCoordVS;
    vec3 LightVS;
    vec3 LightDir;
    vec2 UV;
    vec3 NormalVS;
    vec3 TangentVS;
    vec3 BitangentVS;
} Out;

void main() {
    vec3 light = passdata.vLightPos.xyz;
    mat4 mvp = passdata.mtxProjection * passdata.mtxView * drawdata[gl_InstanceIndex].mtxModel;
    vec4 posVS = ( passdata.mtxView * drawdata[gl_InstanceIndex].mtxModel ) * vec4( inPosition, 1.0 );
    
    //output the position of each vertex
    gl_Position = mvp * vec4( inPosition, 1.0 );
    gl_Position.y = -gl_Position.y;

	mat3 mNormal = mat3( passdata.mtxView * drawdata[gl_InstanceIndex].mtxNormal );
	
	vec4 localTangent = inTangent * 2.0 - 1.0;
	vec3 localNormal = inNormal * 2.0 - 1.0;

	vec3 T = normalize( mNormal * localTangent.xyz );
	vec3 N = normalize( mNormal * localNormal );
	T = normalize(T - dot( T, N ) * N);
	vec3 B = normalize( cross( N, T ) * localTangent.w );

    Out.NormalVS = N;
    Out.TangentVS = T;
    Out.BitangentVS = B;
    Out.Color = drawdata[gl_InstanceIndex].color;
    Out.FragCoordVS = posVS.xyz;
    Out.LightVS = ( passdata.mtxView * vec4( light, 1.0 ) ).xyz;
    Out.LightDir = ( passdata.mtxView * vec4( 0.5,-0.5,0.0,0 ) ).xyz;
    Out.UV = inUV;
}
