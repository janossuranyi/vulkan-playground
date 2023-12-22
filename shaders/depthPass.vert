#version 450
#extension GL_GOOGLE_include_directive : require
//#extension GL_ARB_draw_instanced : enable

layout(location = 0) in vec4 inPosition;

#include "globals.glsl"

layout(set = 2, binding = 0) readonly buffer g_rbObjects_Layout {
	S_OBJECT a[];
} g_rbObjects;

layout(set = 2, binding = 1) readonly buffer g_rbMatrixes_Layout {
	S_MATRIXES a[];
} g_rbMatrixes;

#define G_MATRIXES g_rbMatrixes.a[ g_rbObjects.a[ gl_InstanceIndex ].iMatrixesIdx ]

void main(void) {
	gl_Position = gvars.mViewProjection * G_MATRIXES.mModel * inPosition;
}