#version 450

layout(location = 0) out vec3 color;

void main(){
	vec2 vertices[3]=vec2[3](vec2(-1,-1), vec2(1,-1), vec2(0, 1));
    vec3 colors[3]=vec3[3](vec3(1,0,0),vec3(0,1,0),vec3(0,0,1));
	gl_Position = vec4(vertices[gl_VertexIndex],0,1);
	color = colors[gl_VertexIndex];
}