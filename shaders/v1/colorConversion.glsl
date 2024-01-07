#ifndef COLOR_CONVERSION_INC
#define COLOR_CONVERSION_INC

//reference: https://en.wikipedia.org/wiki/SRGB
vec3 linearTosRGB(vec3 linear){
    vec3 sRGBLo = linear * 12.92;
    vec3 sRGBHi = (pow(abs(linear), vec3(1.0/2.4)) * 1.055)  - 0.055;
    vec3 sRGB;
    sRGB.x = linear.x  <= 0.0031308 ? sRGBLo.x : sRGBHi.x;
    sRGB.y = linear.y  <= 0.0031308 ? sRGBLo.y : sRGBHi.y;
    sRGB.z = linear.z  <= 0.0031308 ? sRGBLo.z : sRGBHi.z;
    return sRGB;
}

vec3 sRGBToLinear(vec3 linear){
    vec3 sRGBLo = linear / 12.92;
    vec3 sRGBHi = (pow(abs(linear + 0.055) / 1.055, vec3(2.4)));
    vec3 sRGB;
    sRGB.x = linear.x  <= 0.004045 ? sRGBLo.x : sRGBHi.x;
    sRGB.y = linear.y  <= 0.004045 ? sRGBLo.y : sRGBHi.y;
    sRGB.z = linear.z  <= 0.004045 ? sRGBLo.z : sRGBHi.z;
    return sRGB;
}

//reference: https://en.wikipedia.org/wiki/YCoCg
vec3 linearToYCoCg(vec3 linear){
    return vec3(
     linear.x * 0.25 + 0.5 * linear.y + 0.25 * linear.z,
     linear.x * 0.5                   - 0.5  * linear.z,
    -linear.x * 0.25 + 0.5 * linear.y - 0.25 * linear.z);
}

vec3 YCoCgToLinear(vec3 YCoCg){
    return vec3(
    YCoCg.x + YCoCg.y - YCoCg.z,
    YCoCg.x           + YCoCg.z,
    YCoCg.x - YCoCg.y - YCoCg.z);
}

float SRGBlinearApprox ( float value ) {
	return value * ( value * ( value * 0.305306011 + 0.682171111 ) + 0.012522878 );
}
vec3 SRGBlinearApprox ( vec3 sRGB ) {
	vec3 outLinear;
	outLinear.r = SRGBlinearApprox( sRGB.r );
	outLinear.g = SRGBlinearApprox( sRGB.g );
	outLinear.b = SRGBlinearApprox( sRGB.b );
	return outLinear;
}
vec4 SRGBlinearApprox ( vec4 sRGBA ) {
	vec4 outLinear = vec4( SRGBlinearApprox( sRGBA.rgb ), 1 );
	outLinear.a = SRGBlinearApprox( sRGBA.a );
	return outLinear;
}

#endif // #ifndef COLOR_CONVERSION_INC