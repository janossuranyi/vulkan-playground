#version 450

const vec2 vertices[3]	= vec2[3](vec2(-1,-1), vec2(1,-1), vec2(0, 1));
const vec3 colors[3]	= vec3[3](vec3(1,0,0),vec3(0,1,0),vec3(0,0,1));

layout(set = 0, binding = 256) uniform globals {
	vec4 rpColorMultiplier;
	vec4 rpColorBias;
	vec4 rpScale;
};

layout(location = 0) out vec3 out_Color0;

void main()
{
	gl_Position = vec4(rpScale.xy * vertices[gl_VertexIndex],0,1);

	out_Color0 = rpColorMultiplier.rgb * colors[gl_VertexIndex] + rpColorBias.rgb;
}