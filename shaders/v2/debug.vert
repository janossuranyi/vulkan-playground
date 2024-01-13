#version 450

layout(location = 0) in vec3 inPosition;

void main(){
    gl_PointSize = 4.0;
    gl_Position = vec4(inPosition, 1.0);
    gl_Position.y = -gl_Position.y;
}