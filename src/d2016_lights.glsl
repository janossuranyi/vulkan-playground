#version 430 core
#extension GL_ARB_shader_clock : enable
void clip( float v ) { if ( v < 0.0 ) { discard; } }
void clip( vec2 v ) { if ( any( lessThan( v, vec2( 0.0 ) ) ) ) { discard; } }
void clip( vec3 v ) { if ( any( lessThan( v, vec3( 0.0 ) ) ) ) { discard; } }
void clip( vec4 v ) { if ( any( lessThan( v, vec4( 0.0 ) ) ) ) { discard; } }

float saturate( float v ) { return clamp( v, 0.0, 1.0 ); }
vec2 saturate( vec2 v ) { return clamp( v, 0.0, 1.0 ); }
vec3 saturate( vec3 v ) { return clamp( v, 0.0, 1.0 ); }
vec4 saturate( vec4 v ) { return clamp( v, 0.0, 1.0 ); }

vec4 tex2D( sampler2D image, vec2 texcoord ) { return texture( image, texcoord.xy ); }
vec4 tex2D( sampler2DShadow image, vec3 texcoord ) { return vec4( texture( image, texcoord.xyz ) ); }
vec4 tex2DARRAY( sampler2DArray image, vec3 texcoord ) { return texture( image, texcoord.xyz ); }

vec4 tex2D( sampler2D image, vec2 texcoord, vec2 dx, vec2 dy ) { return textureGrad( image, texcoord.xy, dx, dy ); }
vec4 tex2D( sampler2DShadow image, vec3 texcoord, vec2 dx, vec2 dy ) { return vec4( textureGrad( image, texcoord.xyz, dx, dy ) ); }
vec4 tex2DARRAY( sampler2DArray image, vec3 texcoord, vec2 dx, vec2 dy ) { return textureGrad( image, texcoord.xyz, dx, dy ); }

vec4 texCUBE( samplerCube image, vec3 texcoord ) { return texture( image, texcoord.xyz ); }
vec4 texCUBE( samplerCubeShadow image, vec4 texcoord ) { return vec4( texture( image, texcoord.xyzw ) ); }
vec4 texCUBEARRAY( samplerCubeArray image, vec4 texcoord ) { return texture( image, texcoord.xyzw ); }

vec4 tex1Dproj( sampler1D image, vec2 texcoord ) { return textureProj( image, texcoord ); }
vec4 tex2Dproj( sampler2D image, vec3 texcoord ) { return textureProj( image, texcoord ); }
vec4 tex3Dproj( sampler3D image, vec4 texcoord ) { return textureProj( image, texcoord ); }

vec4 tex1Dbias( sampler1D image, vec4 texcoord ) { return texture( image, texcoord.x, texcoord.w ); }
vec4 tex2Dbias( sampler2D image, vec4 texcoord ) { return texture( image, texcoord.xy, texcoord.w ); }
vec4 tex2DARRAYbias( sampler2DArray image, vec4 texcoord ) { return texture( image, texcoord.xyz, texcoord.w ); }
vec4 tex3Dbias( sampler3D image, vec4 texcoord ) { return texture( image, texcoord.xyz, texcoord.w ); }
vec4 texCUBEbias( samplerCube image, vec4 texcoord ) { return texture( image, texcoord.xyz, texcoord.w ); }
vec4 texCUBEARRAYbias( samplerCubeArray image, vec4 texcoord, float bias ) { return texture( image, texcoord.xyzw, bias); }

vec4 tex1Dlod( sampler1D image, vec4 texcoord ) { return textureLod( image, texcoord.x, texcoord.w ); }
vec4 tex2Dlod( sampler2D image, vec4 texcoord ) { return textureLod( image, texcoord.xy, texcoord.w ); }
vec4 tex2DARRAYlod( sampler2DArray image, vec4 texcoord ) { return textureLod( image, texcoord.xyz, texcoord.w ); }
vec4 tex3Dlod( sampler3D image, vec4 texcoord ) { return textureLod( image, texcoord.xyz, texcoord.w ); }
vec4 texCUBElod( samplerCube image, vec4 texcoord ) { return textureLod( image, texcoord.xyz, texcoord.w ); }
vec4 texCUBEARRAYlod( samplerCubeArray image, vec4 texcoord, float lod ) { return textureLod( image, texcoord.xyzw, lod ); }

vec4 tex2DGatherRed( sampler2D image, vec2 texcoord ) { return textureGather( image, texcoord, 0 ); }
vec4 tex2DGatherGreen( sampler2D image, vec2 texcoord ) { return textureGather( image, texcoord, 1 ); }
vec4 tex2DGatherBlue( sampler2D image, vec2 texcoord ) { return textureGather( image, texcoord, 2 ); }
vec4 tex2DGatherAlpha( sampler2D image, vec2 texcoord ) { return textureGather( image, texcoord, 3 ); }

vec4 tex2DGatherOffsetRed( sampler2D image, vec2 texcoord, const ivec2 v0 ) { return textureGatherOffset( image, texcoord, v0, 0 ); }
vec4 tex2DGatherOffsetGreen( sampler2D image, vec2 texcoord, const ivec2 v0 ) { return textureGatherOffset( image, texcoord, v0, 1 ); }
vec4 tex2DGatherOffsetBlue( sampler2D image, vec2 texcoord, const ivec2 v0 ) { return textureGatherOffset( image, texcoord, v0, 2 ); }
vec4 tex2DGatherOffsetAlpha( sampler2D image, vec2 texcoord, const ivec2 v0 ) { return textureGatherOffset( image, texcoord, v0, 3 ); }

#define tex2DGatherOffsetsRed( image, texcoord, v0, v1, v2, v3 ) textureGatherOffsets( image, texcoord, ivec2[]( v0, v1, v2, v3 ), 0 )
#define tex2DGatherOffsetsGreen( image, texcoord, v0, v1, v2, v3 ) textureGatherOffsets( image, texcoord, ivec2[]( v0, v1, v2, v3 ), 1 )
#define tex2DGatherOffsetsBlue( image, texcoord, v0, v1, v2, v3 ) textureGatherOffsets( image, texcoord, ivec2[]( v0, v1, v2, v3 ), 2 )
#define tex2DGatherOffsetsAlpha( image, texcoord, v0, v1, v2, v3 ) textureGatherOffsets( image, texcoord, ivec2[]( v0, v1, v2, v3 ), 3 )

uniform vec4 _fa_freqHigh [3];
uniform vec4 _fa_freqLow [9];
uniform lightparms1_ubo { vec4 lightparms1[4096]; };
uniform lightparms2_ubo { vec4 lightparms2[4096]; };
uniform lightparms3_ubo { vec4 lightparms3[4096]; };
uniform sampler2D samp_tex2;
uniform sampler2D samp_tex0;
uniform sampler2D samp_tex1;
uniform samplerCubeArray samp_envprobesmaparray;
struct clusternumlights_t {
	int offset ;
	int numItems ;
};
struct light_t {
	vec3 pos ;
	uint lightParms ;
	vec4 posShadow ;
	vec4 falloffR ;
	vec4 projS ;
	vec4 projT ;
	vec4 projQ ;
	uvec4 scaleBias ;
	vec4 boxMin ;
	vec4 boxMax ;
	vec4 areaPlane ;
	uint lightParms2 ;
	uint colorPacked ;
	float specMultiplier ;
	float shadowBleedReduce ;
};
layout( std430 ) buffer clusternumlights_b {
	restrict readonly clusternumlights_t clusternumlights[ ];
};
layout( std430 ) buffer clusterlightsid_b {
	restrict readonly uint clusterlightsid[ ];
};

in vec4 gl_FragCoord;

out vec4 out_FragColor0;

float asfloat ( uint x ) { return uintBitsToFloat( x ); }
float asfloat ( int x ) { return intBitsToFloat( x ); }
uint asuint ( float x ) { return floatBitsToUint( x ); }
vec2 asfloat ( uvec2 x ) { return uintBitsToFloat( x ); }
vec2 asfloat ( ivec2 x ) { return intBitsToFloat( x ); }
uvec2 asuint ( vec2 x ) { return floatBitsToUint( x ); }
vec3 asfloat ( uvec3 x ) { return uintBitsToFloat( x ); }
vec3 asfloat ( ivec3 x ) { return intBitsToFloat( x ); }
uvec3 asuint ( vec3 x ) { return floatBitsToUint( x ); }
vec4 asfloat ( uvec4 x ) { return uintBitsToFloat( x ); }
vec4 asfloat ( ivec4 x ) { return intBitsToFloat( x ); }
uvec4 asuint ( vec4 x ) { return floatBitsToUint( x ); }
float fmax3 ( float f1, float f2, float f3 ) { return max( f1, max( f2, f3 ) ); }
float fmin3 ( float f1, float f2, float f3 ) { return min( f1, min( f2, f3 ) ); }
vec4 sqr ( vec4 x ) { return ( x * x ); }
vec3 sqr ( vec3 x ) { return ( x * x ); }
vec2 sqr ( vec2 x ) { return ( x * x ); }
float sqr ( float x ) { return ( x * x ); }
float ApproxLog2 ( float f ) {
	return ( float( asuint( f ) ) / ( 1 << 23 ) - 127 );
}
float ApproxExp2 ( float f ) {
	return asfloat( uint( ( f + 127 ) * ( 1 << 23 ) ) );
}
uint packR8G8B8A8 ( vec4 value ) {
	value = saturate( value );
	return ( ( ( uint( value.x * 255.0 ) ) << 24 ) | ( ( uint( value.y * 255.0 ) ) << 16 ) | ( ( uint( value.z * 255.0 ) ) << 8 ) | ( uint( value.w * 255.0 ) ) );
}
vec4 unpackR8G8B8A8 ( uint value ) {
	return vec4( ( value >> 24 ) & 0xFF, ( value >> 16 ) & 0xFF, ( value >> 8 ) & 0xFF, value & 0xFF ) / 255.0;
}
vec4 unpackUintR8G8B8A8 ( uint value ) {
	return vec4( ( value >> 24 ) & 0xFF, ( value >> 16 ) & 0xFF, ( value >> 8 ) & 0xFF, value & 0xFF );
}
uint packR10G10B10 ( vec3 value ) {
	value = saturate( value );
	return ( ( uint( value.x * 1023.0 ) << 20 ) | ( uint( value.y * 1023.0 ) << 10 ) | ( uint( value.z * 1023.0 ) ) );
}
vec3 unpackR10G10B10 ( uint value ) {
	return vec3( ( value >> 20 ) & 0x3FF, ( value >> 10 ) & 0x3FF, ( value ) & 0x3FF ) / 1023.0;
}
uint packRGBE ( vec3 value ) {
	const float sharedExp = ceil( ApproxLog2( max( max( value.r, value.g ), value.b ) ) );
	return packR8G8B8A8( saturate( vec4( value / ApproxExp2( sharedExp ), ( sharedExp + 128.0 ) / 255.0 ) ) );
}
vec3 unpackRGBE ( uint value ) {
	const vec4 rgbe = unpackR8G8B8A8( value );
	return rgbe.rgb * ApproxExp2( rgbe.a * 255.0 - 128.0 );
}
vec2 screenPosToTexcoord ( vec2 pos, vec4 bias_scale ) { return ( pos * bias_scale.zw + bias_scale.xy ); }
vec2 screenPosToTexcoord ( vec2 pos, vec4 bias_scale, vec4 resolution_scale ) { return ( ( pos * bias_scale.zw + bias_scale.xy ) * resolution_scale.xy ); }
float GetLuma ( vec3 c ) {
	return dot( c, vec3( 0.2126, 0.7152, 0.0722 ) );
}
vec3 environmentBRDF ( float NdotV, float smoothness, vec3 f0 ) {
	const float t1 = 0.095 + smoothness * ( 0.6 + 4.19 * smoothness );
	const float t2 = NdotV + 0.025;
	const float t3 = 9.5 * smoothness * NdotV;
	const float a0 = t1 * t2 * ApproxExp2( 1 - 14 * NdotV );
	const float a1 = 0.4 + 0.6 * (1 - ApproxExp2( -t3 ) );
	return mix( vec3( a0 ), vec3( a1 ), f0.xyz );
}
struct lightingInput_t {
	vec3 albedo;
	vec3 colorMask;
	vec3 specular;
	float smoothness;
	float maskSSS;
	float thickness;
	vec3 normal;
	vec3 normalTS;
	vec3 normalSSS;
	vec3 lightmap;
	vec3 emissive;
	float alpha;
	float ssdoDiffuseMul;
	vec3 view;
	vec3 position;
	vec4 texCoords;
	vec4 fragCoord;
	mat3x3 invTS;
	vec3 ambient_lighting;
	vec3 diffuse_lighting;
	vec3 specular_lighting;
	vec3 output_lighting;
	uint albedo_packed;
	uint specular_packed;
	uint diffuse_lighting_packed;
	uint specular_lighting_packed;
	uint normal_packed;
	uvec4 ticksDecals;
	uvec4 ticksProbes;
	uvec4 ticksLights;
};
float GetLinearDepth ( float ndcZ, vec4 projectionMatrixZ, float rcpFarZ, bool bFirstPersonArmsRescale ) {
	float linearZ = projectionMatrixZ.w / ( ndcZ + projectionMatrixZ.z );
	if ( bFirstPersonArmsRescale ) {
		linearZ *= linearZ < 1.0 ? 10.0 : 1.0;
	}
	return linearZ * rcpFarZ;
}
vec2 OctWrap ( vec2 v ) {
	return ( 1.0 - abs( v.yx ) ) * vec2( ( v.x >= 0.0 ? 1.0 : -1.0 ), ( v.y >= 0.0 ? 1.0 : -1.0 ) );
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
vec2 SmoothnessDecode ( float s ) {
	const float expanded_s = s * 2.0 - 1.0;
	return vec2( sqr( expanded_s ), expanded_s > 0 ? 1.0 : 0.0 );
}
void main() {
	{
		vec2 tc = screenPosToTexcoord( gl_FragCoord.xy, _fa_freqHigh[0 ] );
		{
			{
				vec4 spec = tex2Dlod( samp_tex2, vec4( tc.xy, 0, 0 ) );
				float smoothness = SmoothnessDecode( spec.w ).x;
				float depth = tex2Dlod( samp_tex0, vec4( tc.xy, 0, 0 ) ).x;
				out_FragColor0 = vec4( 0, 0, 0, 0 ); if( dot( spec.xyz, vec3( 1 ) ) > 0 && depth < 1.0 ) {
					lightingInput_t inputs = lightingInput_t( vec3(0,0,0), vec3(0,0,0), vec3(0,0,0), 0, 0, 0, vec3(0,0,0), vec3(0,0,1), vec3(0,0,1), vec3(0,0,0), vec3(0,0,0), 1, 1, vec3(0,0,0), vec3(0,0,0), vec4(0,0,0,0), vec4(0,0,0,0), mat3x3( vec3(1,0,0), vec3(0,1,0), vec3(0,0,1) ), vec3(0,0,0), vec3(0,0,0), vec3(0,0,0), vec3(0,0,0), 0, 0, 0, 0, 0, uvec4(0,0,0,0), uvec4(0,0,0,0), uvec4(0,0,0,0) );
					vec3 frustumVecX0 = mix( _fa_freqLow[0 ].xyz, _fa_freqLow[1 ].xyz, tc.x * _fa_freqLow[2 ].z);
					vec3 frustumVecX1 = mix( _fa_freqLow[3 ].xyz, _fa_freqLow[4 ].xyz, tc.x * _fa_freqLow[2 ].z);
					vec3 frustumVec = mix( frustumVecX1.xyz, frustumVecX0.xyz, tc.y* _fa_freqLow[2 ].w);
					inputs.albedo = vec3( 1 );
					inputs.fragCoord.xy = vec2( gl_FragCoord.xy.xy );
					vec4 wPosM = vec4( tc.xy, depth, 1.0 );
					float zLinear = GetLinearDepth( depth, _fa_freqLow[5 ], 1.0, false );
					vec3 world_pos = _fa_freqLow[6 ].xyz + frustumVec.xyz * zLinear;
					inputs.fragCoord.z = depth;
					inputs.position = world_pos.xyz;
					inputs.view = normalize( _fa_freqLow[6 ].xyz - world_pos.xyz );
					inputs.normal = NormalOctDecode( tex2Dlod( samp_tex1, vec4( tc.xy, 0, 0 ) ).xy, false );
					inputs.specular = spec.xyz * spec.xyz;
					inputs.smoothness = smoothness;
					{
						uint skipStaticModel = _fa_freqHigh[1 ].x > 0 ? ( 1 << 3 ) : 0;
						uint clusterOffset;
						{
							{
								float NUM_CLUSTERS_X = 16;
								float NUM_CLUSTERS_Y = 8;
								float NUM_CLUSTERS_Z = 24;
								vec3 clusterCoordinate;
								clusterCoordinate.xy = screenPosToTexcoord( inputs.fragCoord.xy, _fa_freqHigh[2 ] ) * _fa_freqLow[2 ].zw;
								clusterCoordinate.xy = floor( clusterCoordinate.xy * vec2( NUM_CLUSTERS_X, NUM_CLUSTERS_Y ) );
								float curr_z = GetLinearDepth( inputs.fragCoord.z, _fa_freqLow[5 ], 1.0, false );
								float slice = log2( max( 1.0, curr_z / _fa_freqLow[7 ].z ) ) * _fa_freqLow[7 ].w;
								clusterCoordinate.z = min( NUM_CLUSTERS_Z - 1, floor( slice ) );
								clusterOffset = uint( clusterCoordinate.x + clusterCoordinate.y * NUM_CLUSTERS_X + clusterCoordinate.z * NUM_CLUSTERS_X * NUM_CLUSTERS_Y );
							}
						};
						{
							{
								inputs.albedo_packed = packR10G10B10( inputs.albedo.xyz );
								inputs.specular_packed = packR10G10B10( inputs.specular.xyz );
								uint lightsMin = 0;
								uint lightsMax = 0;
								uint decalsMin = 0;
								uint decalsMax = 0;
								uint probesMin = 0;
								uint probesMax = 0;
								{
									{
										int MAX_LIGHTS_PER_CLUSTER = 256;
										clusternumlights_t cluster = clusternumlights[ clusterOffset ];
										int dataOffset = cluster.offset & ( ~ ( cluster.offset >> 31 ) );
										int numItems = cluster.numItems & ( ~ ( cluster.numItems >> 31 ) );
										lightsMin = ( dataOffset >> 16 ) * MAX_LIGHTS_PER_CLUSTER;
										lightsMax = lightsMin + ( ( numItems ) & 0x000000FF );
										decalsMin = lightsMin;
										decalsMax = decalsMin + ( ( ( numItems ) >> 8 ) & 0x000000FF );
										probesMin = lightsMin;
										probesMax = probesMin + ( ( ( numItems ) >> 16 ) & 0x000000FF );
									}
								};
								vec3 ambient = vec3( 0 );
								ambient += inputs.emissive;
								inputs.smoothness = dot( inputs.emissive.xyz, vec3( 1 ) ) > 0.0 ? -inputs.smoothness : inputs.smoothness;
								ambient = mix( GetLuma( ambient.xyz ).xxx, ambient.xyz, _fa_freqLow[8 ].www );
								inputs.ambient_lighting = ambient;
								inputs.diffuse_lighting_packed = packRGBE( ambient );
								float probes_dst_alpha = 1.0;
								for ( uint probeIdx = probesMin; probeIdx < probesMax; ) {
									uint temp = clusterlightsid[ probeIdx ].x;
									uint divergentProbeId = ( ( ( temp ) >> 24 ) & 0x00000FFF );
									uint probe_id;
									{
										probe_id = divergentProbeId; if ( probe_id >= divergentProbeId ) {
											++probeIdx;
										}
									};
									{ light_t lightParms;
										{
											int lightParms1Size = 4;
											int lightParms2Size = 4;
											int lightParms3Size = 3;
											lightParms.pos = lightparms1[ lightParms1Size * ( probe_id ) + 0 ].xyz;
											lightParms.lightParms = asuint( lightparms1[ lightParms1Size * ( probe_id ) + 0 ].w );
											lightParms.posShadow = lightparms1[ lightParms1Size * ( probe_id ) + 1 ];
											lightParms.falloffR = lightparms1[ lightParms1Size * ( probe_id ) + 2 ];
											lightParms.projS = lightparms2[ lightParms2Size * ( probe_id ) + 0 ];
											lightParms.projT = lightparms2[ lightParms2Size * ( probe_id ) + 1 ];
											lightParms.projQ = lightparms2[ lightParms2Size * ( probe_id ) + 2 ];
											lightParms.scaleBias.x = asuint( lightparms1[ lightParms1Size * ( probe_id ) + 3 ].x );
											lightParms.scaleBias.y = asuint( lightparms1[ lightParms1Size * ( probe_id ) + 3 ].y );
											lightParms.scaleBias.z = asuint( lightparms1[ lightParms1Size * ( probe_id ) + 3 ].z );
											lightParms.scaleBias.w = asuint( lightparms1[ lightParms1Size * ( probe_id ) + 3 ].w );
											lightParms.boxMin = lightparms3[ lightParms3Size * ( probe_id ) + 0 ];
											lightParms.boxMax = lightparms3[ lightParms3Size * ( probe_id ) + 1 ];
											lightParms.areaPlane = lightparms3[ lightParms3Size * ( probe_id ) + 2 ];
											lightParms.lightParms2 = asuint( lightparms2[ lightParms2Size * ( probe_id ) + 3 ].x );
											lightParms.colorPacked = asuint( lightparms2[ lightParms2Size * ( probe_id ) + 3 ].y );
											lightParms.specMultiplier = lightparms2[ lightParms2Size * ( probe_id ) + 3 ].z;
											lightParms.shadowBleedReduce = lightparms2[ lightParms2Size * ( probe_id ) + 3 ].w;
										};
										vec3 projTC = vec3( ( inputs.position.x * lightParms.projS.x + ( inputs.position.y * lightParms.projS.y + ( inputs.position.z * lightParms.projS.z + lightParms.projS.w ) ) ),
										( inputs.position.x * lightParms.projT.x + ( inputs.position.y * lightParms.projT.y + ( inputs.position.z * lightParms.projT.z + lightParms.projT.w ) ) ),
										( inputs.position.x * lightParms.projQ.x + ( inputs.position.y * lightParms.projQ.y + ( inputs.position.z * lightParms.projQ.z + lightParms.projQ.w ) ) ) );
										projTC.xy /= projTC.z;
										projTC.z = inputs.position.x * lightParms.falloffR.x + ( inputs.position.y * lightParms.falloffR.y + ( inputs.position.z * lightParms.falloffR.z + lightParms.falloffR.w ) );
										float clip_min = 1.0 / 255.0; if ( fmin3( projTC.x, projTC.y, projTC.z ) <= clip_min || fmax3( projTC.x, projTC.y, projTC.z ) >= 1.0 - clip_min ) {
											continue;
										}
										uint light_parms = lightParms.lightParms;
										vec3 light_color = unpackRGBE( lightParms.colorPacked );
										float light_spec_multiplier = lightParms.specMultiplier;
										vec3 light_position = lightParms.pos;
										float light_probe_innerfalloff = unpackR8G8B8A8( light_parms ).y;
										vec3 norm_pos_cube = projTC.xyz * 2.0 - vec3( 1.0 );
										vec3 norm_pos_sphere = norm_pos_cube * sqrt( vec3( 1.0 ) - norm_pos_cube.yzx * norm_pos_cube.yzx * 0.5 - norm_pos_cube.zxy * norm_pos_cube.zxy * 0.5 + ( norm_pos_cube.yzx * norm_pos_cube.yzx * norm_pos_cube.zxy * norm_pos_cube.zxy / 3.0 ) );
										float light_attenuation = saturate( 1 - ( length( norm_pos_sphere.xyz ) - light_probe_innerfalloff ) / ( 1.0 - light_probe_innerfalloff + 1e-6 ) );
										light_attenuation *= light_attenuation;
										light_attenuation *= lightParms.boxMin.w * probes_dst_alpha;
										probes_dst_alpha *= ( 1 - light_attenuation ); if( light_attenuation <= clip_min * 4 ) {
											continue;
										}
										vec3 light_color_final = light_color;
										light_color_final = mix( GetLuma( light_color_final.xyz ).xxx, light_color_final.xyz, _fa_freqLow[8 ].www );
										{
											float light_probe_id = unpackUintR8G8B8A8( light_parms ).z;
											vec3 diffEnvProbe = vec3( 0 );
											vec3 specEnvProbe = vec3( 0 );
											vec3 normal = inputs.normal.xyz;
											normal.x = dot( lightParms.posShadow.xyzw.xy, inputs.normal.xy );
											normal.y = dot( lightParms.posShadow.xyzw.zw, inputs.normal.xy );
											vec3 R = reflect( -inputs.view, inputs.normal );
											vec3 bmax = ( lightParms.boxMax.xyz.xyz - inputs.position.xyz ) / R;
											vec3 bmin = ( lightParms.boxMin.xyz.xyz - inputs.position.xyz ) / R;
											vec3 bminmax = max( bmax, bmin );
											float intersection_dist = fmin3( bminmax.x, bminmax.y, bminmax.z) ;
											vec3 intersection_pos = inputs.position.xyz + R.xyz * intersection_dist;
											vec3 LRtmp = normalize( intersection_pos - light_position );
											vec3 LR = LRtmp.xyz;
											LR.x = dot( lightParms.posShadow.xyzw.xy, LRtmp.xy );
											LR.y = dot( lightParms.posShadow.xyzw.zw, LRtmp.xy );
											float num_mips = 6;
											float mip_level = num_mips - num_mips * abs( inputs.smoothness );
											specEnvProbe = texCUBEARRAYlod( samp_envprobesmaparray, vec4( LR.xyz, light_probe_id ), mip_level ).xyz;
											float NdotV = saturate( dot( inputs.view.xyz, inputs.normal ) );
											vec3 envBRDF = environmentBRDF( NdotV, abs( inputs.smoothness ), unpackR10G10B10( inputs.specular_packed ) );
											specEnvProbe *= envBRDF * light_spec_multiplier;
											diffEnvProbe = mix( GetLuma( diffEnvProbe.xyz ).xxx, diffEnvProbe.xyz, _fa_freqLow[8 ].www );
											specEnvProbe = mix( GetLuma( specEnvProbe.xyz ).xxx, specEnvProbe.xyz, _fa_freqLow[8 ].www );
											inputs.specular_lighting_packed = packRGBE( specEnvProbe * light_color_final * saturate( light_attenuation ) + unpackRGBE( inputs.specular_lighting_packed ) );
										}; if( probes_dst_alpha <= clip_min * 4 ) {
											break;
										}
									};
								}
								inputs.diffuse_lighting = unpackRGBE( inputs.diffuse_lighting_packed ) * unpackR10G10B10( inputs.albedo_packed );
								inputs.specular_lighting = unpackRGBE( inputs.specular_lighting_packed );
								inputs.output_lighting = inputs.diffuse_lighting + inputs.specular_lighting;
							};
						}
					};
					out_FragColor0.rgb = inputs.specular_lighting.xyz;
				}
			}
		};
	}
}