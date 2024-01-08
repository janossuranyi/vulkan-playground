#ifndef FUNCTIONS_INC_
#define FUNCTIONS_INC_

float asfloat ( uint x ) { return uintBitsToFloat( x ); }
uint asuint ( float x ) { return floatBitsToUint( x ); }
int asint ( float x ) { return floatBitsToInt( x ) ; }

float saturate(float x) { return clamp(x, 0.0, 1.0); }
vec2 saturate(vec2 x) { return clamp(x, 0.0, 1.0); }
vec3 saturate(vec3 x) { return clamp(x, 0.0, 1.0); }
vec4 saturate(vec4 x) { return clamp(x, 0.0, 1.0); }

float ApproxPow ( float fBase, float fPower ) {
	return asfloat( uint( fPower * float( asuint( fBase ) ) - ( fPower - 1 ) * 127 * ( 1 << 23 ) ) );
}

vec2 OctWrap ( vec2 v ) {
	return ( 1.0 - abs( v.yx ) ) * vec2( sign(v.x) , sign(v.y) );
}

vec3 NormalOctDecode ( vec2 encN, bool expand_range ) {
	if ( expand_range ) {
		encN = encN * 2.0 - 1.0;
	}
	vec3 n;
	n.z = 1.0 - abs( encN.x ) - abs( encN.y );
	n.xy = n.z >= 0.0 ? encN.xy : OctWrap( encN.xy );
	n = normalize( n );
	return n;
}

vec2 NormalOctEncode ( vec3 n, bool compress_range ) {
	n /= ( abs( n.x ) + abs( n.y ) + abs( n.z ) );
	n.xy = n.z >= 0.0 ? n.xy : OctWrap( n.xy );
	if ( compress_range ) {
		n.xy = n.xy * 0.5 + 0.5;
	}
	return n.xy;
}

float asfloat(int x)
{
    return intBitsToFloat(x);
}

float sqrtIEEEIntApproximation(float inX, int inSqrtConst)
{
    int x = asint(inX);
    x = inSqrtConst + (x >> 1);
    return asfloat(x);
}

float fastSqrtNR0(float inX)
{
    const int param_1 = 532487669;
    return sqrtIEEEIntApproximation(inX, param_1);
}

float SmoothnessEncode(float s)
{
    float param = abs(s);
    float s1 = fastSqrtNR0(param);
    s1 = s > 0.0 ? s1 : -s1;
    return (s1 * 0.5) + 0.5;
}

#endif
