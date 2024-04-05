#ifndef COLOR_CONVERSION_INC
#define COLOR_CONVERSION_INC


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
const mat3 fromP3to2020 = mat3(
    vec3(0.753845, 0.198593, 0.047562),
    vec3(0.0457456, 0.941777, 0.0124772),
    vec3(-0.00121055, 0.0176041, 0.983607)
);
const mat3 fromExpanded709to2020  = mat3(
    vec3(0.6274040, 0.3292820, 0.0433136),
    vec3(0.0457456, 0.941777, 0.0124772),
    vec3(-0.00121055, 0.0176041, 0.983607)
);

// srgb primaries
// Also same with Rec.709
const mat3 sRGB_2_XYZ = mat3(
    0.4124564, 0.2126729, 0.0193339,
    0.3575761, 0.7151522, 0.1191920,
    0.1804375, 0.0721750, 0.9503041
);
const mat3 XYZ_2_sRGB = mat3(
     3.2404542,-0.9692660, 0.0556434,
    -1.5371385, 1.8760108,-0.2040259,
    -0.4985314, 0.0415560, 1.0572252
);

// REC 2020 primaries
const mat3 XYZ_2_Rec2020 =  mat3(
	 1.7166084, -0.6666829, 0.0176422,
	-0.3556621,  1.6164776, -0.0427763,
	-0.2533601,  0.0157685, 0.94222867	
);

const mat3 Rec2020_2_XYZ = mat3(
	0.6369736, 0.2627066, 0.0000000,
	0.1446172, 0.6779996, 0.0280728,
	0.1688585, 0.0592938, 1.0608437
);


const mat3 sRGB_2_Rec2020 = XYZ_2_Rec2020 * sRGB_2_XYZ;
const mat3 Rec2020_2_sRGB = XYZ_2_sRGB * Rec2020_2_XYZ;

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