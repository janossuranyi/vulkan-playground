#version 450

const vec2 vertices[3]	= vec2[3](vec2(-1,-1), vec2(1,-1), vec2(0, 1));
const vec3 colors[3]	= vec3[3](vec3(1,0,0),vec3(0,1,0),vec3(0,0,1));

layout(location = 0) out vec3 color0;
layout(constant_id = 1) const float VSCALE = 1.0;
layout(constant_id = 2) const int PARM_COUNT = 2;

const vec2 vscale = vec2(VSCALE);
const bool paramcolors = PARM_COUNT > 2;

layout(set = 0, binding = 0) uniform stc_ubo_Params {
	vec4 params[PARM_COUNT];
};

const mat3 from709toDisplayP3 = mat3(    
    0.822461969, 0.033194199, 0.017082631,
    0.1775380,   0.9668058,   0.0723974,
    0.0000000,   0.0000000,   0.9105199
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

void main()
{
	gl_Position = vec4(vscale * vertices[gl_VertexIndex],0,1);

	if (paramcolors)
	{
		color0	= PQinverseEOTF( from709toDisplayP3 * params[gl_VertexIndex].xyz );
	} 
	else 
	{
		color0 	= colors[gl_VertexIndex];
	}
}