#version 450
#extension GL_GOOGLE_include_directive : require

layout(location = 0) in vec4 inPosition;
layout(location = 1) in vec2 inTexCoord;
layout(location = 2) in vec4 inNormal;
layout(location = 3) in vec4 inTangent;
layout(location = 4) in vec4 inColor;

layout(location = 0) out INTERFACE {
	vec4 color;
	vec2 uv;
	vec3 tangent;
	vec3 normal;
	vec3 bitangent;
	vec4 fragPosVS;
	float objectIndex;
} Out;

#include "globals.glsl"

layout(set = 2, binding = 0) readonly buffer g_rbObjects_Layout {
	S_OBJECT g_rbObjects[];
};

layout(set = 2, binding = 1) readonly buffer g_rbMatrixes_Layout {
	S_MATRIXES g_rbMatrixes[];
};

#define G_MATRIXES g_rbMatrixes[ g_rbObjects[ gl_InstanceIndex ].iMatrixesIdx ]

void main()
{
	//const array of positions for the triangle
	const vec3 positions[3] = vec3[3](
		vec3( 1.0, 1.0, 0.0),
		vec3(-1.0, 1.0, 0.0),
		vec3( 0.0,-1.0, 0.0)
	);

	const vec2 UVs[3] = vec2[3](
		vec2( 1.0, 1.0 ),
		vec2( 0.0, 1.0 ),
		vec2( 0.5, 0.0 )
	);

	const vec3 colors[3] = vec3[3](
		vec3( 1.0, 0.0, 0.0),
		vec3( 0.0, 1.0, 0.0),
		vec3( 0.0, 0.0, 1.0)
	);

	const vec3 lightPos = vec3(0,0.5,0);

	float min = 0.05;
	float max = 1.0;

	//output the position of each vertex
	mat3 mNormal = mat3(gvars.mView * G_MATRIXES.mNormal);
	
	vec4 localTangent = inTangent * 2.0 - 1.0;
	vec3 localNormal = inNormal.xyz * 2.0 - 1.0;

	vec3 T = normalize(mNormal * localTangent.xyz);
	vec3 N = normalize(mNormal * localNormal);
	T = normalize(T - dot( T, N ) * N);
	vec3 B = normalize( cross( N, T ) * localTangent.w );

	Out.normal		= N;
	Out.tangent		= T;
	Out.bitangent	= B;
	Out.color		= inColor;
	Out.uv			= inTexCoord;
	Out.fragPosVS	= gvars.mView * G_MATRIXES.mModel * inPosition;
	Out.objectIndex	= float(gl_InstanceIndex) + 0.5;
	gl_Position = gvars.mViewProjection * G_MATRIXES.mModel * inPosition;
}
