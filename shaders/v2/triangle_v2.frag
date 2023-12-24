#version 450

layout(location = 0) out vec4 outColor0;
layout(location = 0) in vec4 inColor;
layout(location = 1) in vec3 inFragCoordVS;
layout(location = 2) in vec3 inLightVS;


void main() {

    vec4 lightColor = vec4(10,10,10,1);
    vec3 ddx = dFdx( inFragCoordVS );
    vec3 ddy = dFdy( inFragCoordVS );
    vec3 N = normalize( cross( ddx, ddy ) );
    float intensity = max(dot(N, inLightVS),0.0);
    vec4 ambient = 0.01*inColor;
    outColor0 = ((inColor * intensity) * lightColor) + ambient;
    outColor0 /= (vec4(1.0) + outColor0);
}