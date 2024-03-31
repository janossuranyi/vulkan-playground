#version 450

const vec2 vertices[3]	= vec2[3](vec2(-1,-1), vec2(1,-1), vec2(0, 1));
const vec3 colors[3]	= vec3[3](vec3(1,0,0),vec3(0,1,0),vec3(0,0,1));

layout(location = 0) out vec3 color0;
layout(constant_id = 1) const float VSCALE = 1.0;
layout(constant_id = 2) const int PARM_COUNT = 1;

const vec2 vscale = vec2(VSCALE);

layout(set = 0, binding = 0) uniform stc_ubo_Params {
	vec4 params[8];
};


void main()
{
	gl_Position = vec4(vscale * vertices[gl_VertexIndex],0,1);

	color0 = params[gl_VertexIndex].xyz;
}