#version 450

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec2 inUV;
layout(location = 2) in vec3 inNormal;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inColor;



layout(set = 0, binding = 0) uniform uPassData {
    mat4 mtxView;
    mat4 mtxProjection;
};

layout(set = 0, binding = 1) uniform uDrawData {
    mat4 mtxModel;
    mat4 mtxNormal;
    vec4 color;
};

layout(location = 0) out INTERFACE {
    vec4 Color;
    vec3 FragCoordVS;
    vec3 LightVS;
    vec2 UV;
    vec3 NormalVS;
    vec3 TangentVS;
    vec3 BitangentVS;
} Out;

void main() {
    vec3 light = vec3(0,2,0);
    mat4 mvp = mtxProjection * mtxView * mtxModel;
    vec4 posVS = (mtxView * mtxModel) * vec4(inPosition, 1.0);
    gl_Position = mvp * vec4(inPosition, 1.0);
    gl_Position.y = -gl_Position.y;

    //output the position of each vertex
	mat3 mNormal =mat3(mtxView * mtxNormal);
	
	vec4 localTangent = inTangent * 2.0 - 1.0;
	vec3 localNormal = inNormal * 2.0 - 1.0;

	vec3 T = normalize(mNormal * localTangent.xyz);
	vec3 N = normalize(mNormal * localNormal);
	T = normalize(T - dot( T, N ) * N);
	vec3 B = normalize( cross( N, T ) * localTangent.w );

    Out.NormalVS = N;
    Out.TangentVS = T;
    Out.BitangentVS = B;
    Out.Color = color;
    Out.FragCoordVS = posVS.xyz;
    Out.LightVS = (mtxView * vec4(light,1.0)).xyz;
    Out.UV = inUV;
}