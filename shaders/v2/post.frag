#version 450
#extension GL_GOOGLE_include_directive : require

#include "../v1/tonemapping.glsl"
#include "../v1/colorConversion.glsl"

layout(location = 0) in vec2 texcoord;

layout(set = 0, binding = 0) uniform sampler2D samp_input;

layout(location = 0) out vec4 fragColor0;

void main() {
    vec4 inColor = texture(samp_input, texcoord);
    inColor.rgb = ACESFitted( inColor.rgb );
    inColor.rgb = linearTosRGB( inColor.rgb );
    fragColor0 = vec4(inColor.rgb, 1.0);
}