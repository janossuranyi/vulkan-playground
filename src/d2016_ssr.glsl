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

uniform vec4 _fa_freqHigh [8];
uniform vec4 _fa_freqLow [13];
uniform sampler2D samp_tex0;
uniform sampler2D samp_tex4;
uniform sampler2D samp_tex1;
uniform sampler2D samp_tex3;
uniform sampler2D samp_tex5;

in vec4 gl_FragCoord;

out vec4 out_FragColor0;
out vec4 out_FragColor1;

float asfloat ( uint x ) { return uintBitsToFloat( x ); }
float asfloat ( int x ) { return intBitsToFloat( x ); }
vec2 asfloat ( uvec2 x ) { return uintBitsToFloat( x ); }
vec2 asfloat ( ivec2 x ) { return intBitsToFloat( x ); }
vec3 asfloat ( uvec3 x ) { return uintBitsToFloat( x ); }
vec3 asfloat ( ivec3 x ) { return intBitsToFloat( x ); }
vec4 asfloat ( uvec4 x ) { return uintBitsToFloat( x ); }
vec4 asfloat ( ivec4 x ) { return intBitsToFloat( x ); }
int firstBitLow ( uint value ) { return findLSB( value ); }
vec4 sqr ( vec4 x ) { return ( x * x ); }
vec3 sqr ( vec3 x ) { return ( x * x ); }
vec2 sqr ( vec2 x ) { return ( x * x ); }
float sqr ( float x ) { return ( x * x ); }
vec4 MatrixMul ( vec3 pos, mat4x4 mat ) {
	return vec4( ( pos.x * mat[0].x + ( pos.y * mat[0].y + ( pos.z * mat[0].z + mat[0].w ) ) ),
	( pos.x * mat[1].x + ( pos.y * mat[1].y + ( pos.z * mat[1].z + mat[1].w ) ) ),
	( pos.x * mat[2].x + ( pos.y * mat[2].y + ( pos.z * mat[2].z + mat[2].w ) ) ),
	( pos.x * mat[3].x + ( pos.y * mat[3].y + ( pos.z * mat[3].z + mat[3].w ) ) ) );
}
vec3 MatrixMul ( vec3 pos, mat3x4 mat ) {
	return vec3( ( pos.x * mat[0].x + ( pos.y * mat[0].y + ( pos.z * mat[0].z + mat[0].w ) ) ),
	( pos.x * mat[1].x + ( pos.y * mat[1].y + ( pos.z * mat[1].z + mat[1].w ) ) ),
	( pos.x * mat[2].x + ( pos.y * mat[2].y + ( pos.z * mat[2].z + mat[2].w ) ) ) );
}
vec4 MatrixMul ( vec3 pos, vec4 matX, vec4 matY, vec4 matZ, vec4 matW ) {
	return vec4( ( pos.x * matX.x + ( pos.y * matX.y + ( pos.z * matX.z + matX.w ) ) ),
	( pos.x * matY.x + ( pos.y * matY.y + ( pos.z * matY.z + matY.w ) ) ),
	( pos.x * matZ.x + ( pos.y * matZ.y + ( pos.z * matZ.z + matZ.w ) ) ),
	( pos.x * matW.x + ( pos.y * matW.y + ( pos.z * matW.z + matW.w ) ) ) );
}
vec3 MatrixMul ( vec3 pos, vec4 matX, vec4 matY, vec4 matZ ) {
	return vec3( ( pos.x * matX.x + ( pos.y * matX.y + ( pos.z * matX.z + matX.w ) ) ),
	( pos.x * matY.x + ( pos.y * matY.y + ( pos.z * matY.z + matY.w ) ) ),
	( pos.x * matZ.x + ( pos.y * matZ.y + ( pos.z * matZ.z + matZ.w ) ) ) );
}
vec4 MatrixMul ( mat4x4 m, vec4 v ) {
	return m * v;
}
vec4 MatrixMul ( vec4 v, mat4x4 m ) {
	return v * m;
}
vec3 MatrixMul ( mat3x3 m, vec3 v ) {
	return m * v;
}
vec3 MatrixMul ( vec3 v, mat3x3 m ) {
	return v * m;
}
vec2 MatrixMul ( mat2x2 m, vec2 v ) {
	return m * v;
}
vec2 MatrixMul ( vec2 v, mat2x2 m ) {
	return v * m;
}
float ApproxExp2 ( float f ) {
	return asfloat( uint( ( f + 127 ) * ( 1 << 23 ) ) );
}
vec2 screenPosToTexcoord ( vec2 pos, vec4 bias_scale ) { return ( pos * bias_scale.zw + bias_scale.xy ); }
vec2 screenPosToTexcoord ( vec2 pos, vec4 bias_scale, vec4 resolution_scale ) { return ( ( pos * bias_scale.zw + bias_scale.xy ) * resolution_scale.xy ); }
vec4 tex2DFetch ( sampler2D image, ivec2 texcoord, int mip ) { return texelFetch( image, texcoord, mip ); }
vec3 environmentBRDF ( float NdotV, float smoothness, vec3 f0 ) {
	const float t1 = 0.095 + smoothness * ( 0.6 + 4.19 * smoothness );
	const float t2 = NdotV + 0.025;
	const float t3 = 9.5 * smoothness * NdotV;
	const float a0 = t1 * t2 * ApproxExp2( 1 - 14 * NdotV );
	const float a1 = 0.4 + 0.6 * (1 - ApproxExp2( -t3 ) );
	return mix( vec3( a0 ), vec3( a1 ), f0.xyz );
}
struct gbuffer_t {
	vec3 world_pos;
	vec3 view;
	vec3 normal;
	vec3 specular;
	vec3 reflection;
	float smoothness;
	float depth;
	vec3 phongEnvBRDF;
};
float GetLinearDepth ( float ndcZ, vec4 projectionMatrixZ, float rcpFarZ, bool bFirstPersonArmsRescale ) {
	float linearZ = projectionMatrixZ.w / ( ndcZ + projectionMatrixZ.z );
	if ( bFirstPersonArmsRescale ) {
		linearZ *= linearZ < 1.0 ? 10.0 : 1.0;
	}
	return linearZ * rcpFarZ;
}
vec3 GetWindowPosZ ( vec3 viewPos, vec4 projection ) {
	return vec3( vec2( 0.5 ) + projection.xy * ( viewPos.xy / viewPos.z ), ( projection.w / viewPos.z ) + projection.z ) ;
}
vec3 GetViewPos ( vec3 winPos, vec4 inverseProjection0, vec4 inverseProjection1 ) {
	return vec3( inverseProjection0.xy * winPos.xy + inverseProjection0.zw, inverseProjection1.z ) / ( inverseProjection1.x * winPos.z + inverseProjection1.y );
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
		{
			vec2 tc = screenPosToTexcoord( gl_FragCoord.xy.xy, _fa_freqHigh[0 ] );
			{
				{
					out_FragColor0 = vec4( 0 );
					out_FragColor1 = vec4( 0 );
					gbuffer_t scene;
					{
						{
							vec3 normal = NormalOctDecode( tex2DFetch( samp_tex0, ivec2( gl_FragCoord.xy.xy ).xy, 0 ).xy, false );
							vec4 spec = tex2DFetch( samp_tex4, ivec2( gl_FragCoord.xy.xy ).xy, 0 );
							scene.normal = normal.xyz;
							mat3x3 invProjT = mat3x3( _fa_freqLow[0 ].xyz, _fa_freqLow[1 ].xyz, _fa_freqLow[2 ].xyz );
							scene.normal = normalize( MatrixMul( invProjT, scene.normal.xyz ) );
							vec2 decodedSmoothness = SmoothnessDecode( spec.w );
							scene.smoothness = decodedSmoothness.y > 0? decodedSmoothness.x : 0;
							scene.specular = sqr( tex2DFetch( samp_tex4, ivec2( gl_FragCoord.xy.xy ).xy, 0 ).xyz );
							scene.depth = tex2DFetch( samp_tex1, ivec2( gl_FragCoord.xy.xy ).xy, 0 ).x;
							float zLinear = GetLinearDepth( scene.depth, _fa_freqLow[3 ], 1.0, true );
							scene.world_pos = GetViewPos( vec3( tc * _fa_freqLow[4 ].zw, scene.depth ), _fa_freqLow[5 ], _fa_freqLow[6 ] );
							scene.view = normalize( - scene.world_pos.xyz );
							scene.reflection = ( reflect( -scene.view.xyz, scene.normal.xyz ) );
							float NdotV = saturate( dot( scene.view.xyz, scene.normal.xyz ) );
							scene.phongEnvBRDF = environmentBRDF( NdotV, scene.smoothness, ( scene.specular ) );
						}
					}
					float smoothness_threshold = _fa_freqHigh[1 ].x;
					float VdovR_threshold = 0.0; if ( scene.smoothness > smoothness_threshold && dot( scene.view.xyz, scene.reflection.xyz ) <= VdovR_threshold && scene.depth < 1.0 ) {
						uvec2 idxPix = uvec2( ivec2( gl_FragCoord.xy.xy ).xy) & 3;
						float jitter = float( ( ( ( 2068378560 * ( 1 - ( idxPix.x >> 1 ) ) + 1500172770 * ( idxPix.x >> 1 ) ) >> ( ( idxPix.y + ( ( idxPix.x & 1 ) << 2 ) ) << 2 ) ) + int( _fa_freqHigh[2 ].w ) ) & 0xF ) / 15.0;
						jitter -= 1.0;
						int max_steps = 16;
						float stride = 1;
						vec3 ray_start_vs = scene.world_pos;
						vec3 ray_end_vs = ray_start_vs.xyz + scene.reflection * GetLinearDepth( scene.depth, _fa_freqLow[3 ], 1, true );
						vec4 hit_color;
						{
							{
								hit_color = vec4( 0 );
								vec3 ray_start = GetWindowPosZ( ray_start_vs, _fa_freqLow[7 ] ) * vec3( _fa_freqLow[4 ].xy, 1 );
								vec3 ray_end = GetWindowPosZ( ray_end_vs, _fa_freqLow[7 ] ) * vec3( _fa_freqLow[4 ].xy, 1 );
								vec3 ray_step = ( stride / float( max_steps ) ) * ( ray_end.xyz - ray_start.xyz ) / length( ray_end.xy - ray_start.xy );
								vec3 ray_pos = ray_start.xyz + ray_step.xyz * jitter;
								float z_thickness = abs( ray_step.z );
								int hit = 0;
								vec3 best_hit = ray_pos;
								float prev_scene_z = ray_start.z; for ( int curr_step = 0; curr_step < max_steps ; curr_step += 4 ) {
									vec4 scene_z4 = 1 - vec4( tex2Dlod( samp_tex3, vec4( ray_pos.xy + ray_step.xy * float( curr_step + 1 ), 0, 0 ) ).x,
									tex2Dlod( samp_tex3, vec4( ray_pos.xy + ray_step.xy * float( curr_step + 2 ), 0, 0 ) ).x,
									tex2Dlod( samp_tex3, vec4( ray_pos.xy + ray_step.xy * float( curr_step + 3 ), 0, 0 ) ).x,
									tex2Dlod( samp_tex3, vec4( ray_pos.xy + ray_step.xy * float( curr_step + 4 ), 0, 0 ) ).x );
									vec4 curr_step4 = vec4( 1, 2, 3, 4 ) + vec4( curr_step );
									uvec4 z_test = uvec4( lessThan( abs( ( ray_pos.zzzz + ray_step.zzzz * curr_step4 ) - scene_z4 - z_thickness.xxxx ), z_thickness.xxxx ) );
									uint z_mask = ( z_test[ 0 ] << 0 ) | ( z_test[ 1 ] << 1 ) | ( z_test[ 2 ] << 2 ) | ( z_test[ 3 ] << 3 ); if ( z_mask > 0 ) {
										int first_hit = firstBitLow( z_mask );
										prev_scene_z = first_hit > 0 ? scene_z4[ max( 0, first_hit - 1 ) ] : prev_scene_z;
										best_hit = ray_pos + ray_step * float( curr_step + first_hit + 1 );
										float z_after = scene_z4[ first_hit ] - best_hit.z;
										float z_before = prev_scene_z - best_hit.z + ray_step.z;
										float w = saturate( z_after / ( z_after - z_before ) );
										vec3 prev_ray_pos = best_hit.xyz - ray_step.xyz;
										best_hit = prev_ray_pos * w + best_hit * ( 1.0 - w );
										hit = curr_step + int( first_hit );
										break;
									}
									prev_scene_z = scene_z4.w;
								} if ( hit > 0 ) {
									vec4 worldPosM = MatrixMul( vec3( best_hit.xy * _fa_freqLow[4 ].zw, best_hit.z ), _fa_freqLow[8 ], _fa_freqLow[9 ], _fa_freqLow[10 ], _fa_freqLow[11 ] );
									worldPosM /= worldPosM.w;
									vec4 tc_reproj = MatrixMul( worldPosM.xyz, _fa_freqHigh[3 ], _fa_freqHigh[4 ], _fa_freqHigh[5 ], _fa_freqHigh[6 ] );
									tc_reproj.xy /= tc_reproj.w;
									tc_reproj.xy = tc_reproj.xy * 0.5 + 0.5;
									tc_reproj.xy *= _fa_freqLow[12 ].xy;
									vec4 finalRefl = vec4( tex2Dlod( samp_tex5, vec4( tc_reproj.xy, 0, 0 ) ).xyz, 1 );
									vec2 dist = ( tc_reproj.xy * _fa_freqLow[12 ].zw ) * 2 - 1;
									float edge_atten = ( smoothstep( 0.0, 0.5, saturate( 1 - dot( dist.xy, dist.xy ) ) ) );
									edge_atten *= ( saturate( ( 1 - ( float( hit ) / 16.0 ) ) * 4.0 ) );
									hit_color = vec4( finalRefl.xyz, edge_atten );
								}
							}
						};
						float smoothness_mask = saturate( saturate( 1- ( ( 1.0 - scene.smoothness ) / ( 1.0 - smoothness_threshold ) ) ) );
						hit_color.w = saturate( hit_color.w * smoothness_mask * _fa_freqHigh[7 ].x );
						out_FragColor0.rgb = max( vec3( 0 ), hit_color.w * hit_color.xyz * scene.phongEnvBRDF );
						out_FragColor1.r = saturate( hit_color.w );
					}
				}
			};
		};
	}
}