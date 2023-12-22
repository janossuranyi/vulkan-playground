#ifndef GLOBALS_INC_
#define GLOBALS_INC_

const float pi = 3.1415926535;
// gloabl shader resources
layout(set = 0, binding = 0) uniform GlobalShaderVariables {
	mat4	mView;
	mat4	mProjection;
	mat4	mInvProjection;
	mat4	mViewProjection;
	vec4	vDisplaySize;
	vec4	vAmbientColor;
	vec4	vLightColor;
	vec4	vFogColor;
	float	fLightRange;
	uint	uFramenumber;
	int		iWhiteImageIndex;
	int		iBlackImageIndex;
	int		iFlatNormalIndex;
	float	fExposureBias;
	float	fLinearAttenuation;
	float	fQuadraticAttenuation;
	float	fFarPlane;
	float	fNearPlane;
	float	fFogDensity;
	int		dummy1;
} gvars;

layout(set = 0, binding = 1) uniform sampler gs_linearRepeat;
layout(set = 0, binding = 2) uniform sampler gs_linearEdgeClamp;
layout(set = 0, binding = 3) uniform sampler gs_nearestEdgeClamp;

layout(set = 1, binding = 0) uniform texture2D[]    g_2DTextures;
layout(set = 1, binding = 0) uniform texture3D[]    g_3DTextures;
layout(set = 1, binding = 0) uniform textureCube[]  g_CubeTextures;

struct S_MATRIXES {
	mat4 mModel;
	mat4 mNormal;
};

struct S_OBJECT {
	int iMatrixesIdx;
	int iMaterialIdx;
};

#endif
