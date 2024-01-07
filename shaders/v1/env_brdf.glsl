#ifndef ENV_BRDF_INC_
#define ENV_BRDF_INC_

float ApproxExp2 ( float f ) {
	return uintBitsToFloat( uint( ( f + 127 ) * ( 1 << 23 ) ) );
}

vec3 environmentBRDF ( float NdotV, float smoothness, vec3 f0 ) {
	const float t1 = 0.095 + smoothness * ( 0.6 + 4.19 * smoothness );
	const float t2 = NdotV + 0.025;
	const float t3 = 9.5 * smoothness * NdotV;
	const float a0 = t1 * t2 * ApproxExp2( 1 - 14 * NdotV );
	const float a1 = 0.4 + 0.6 * (1 - ApproxExp2( -t3 ) );
	return mix( vec3( a0 ), vec3( a1 ), f0.xyz );
}

#endif