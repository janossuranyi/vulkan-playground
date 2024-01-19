#version 430 core
#extension GL_ARB_shader_clock : enable
//This is a compute Shader

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

uniform vec4 _ca_freqHigh [3];
uniform vec4 _ca_freqLow [16];
layout( local_size_x = 8, local_size_y = 8, local_size_z = 1 ) in ;
uniform sampler2D samp_viewdepthmap;
uniform sampler2D samp_tex4;
uniform sampler2D samp_tex0;
uniform sampler2D samp_tex2;
uniform sampler2D samp_tex3;
uniform sampler2D samp_tex1;
layout( binding = 0, r11f_g11f_b10f ) uniform image2D ib_uavdeferredcomposite0 ;
layout( binding = 1, r11f_g11f_b10f ) uniform image2D ib_uavdeferredcomposite1 ;
float dot4 ( vec4 a, vec4 b ) { return dot( a, b ); }
float dot4 ( vec2 a, vec4 b ) { return dot( vec4( a, 0.0, 1.0 ), b ); }
float GetLinearDepth ( float ndcZ, vec4 projectionMatrixZ, float rcpFarZ, bool bFirstPersonArmsRescale ) {
	float linearZ = projectionMatrixZ.w / ( ndcZ + projectionMatrixZ.z );
	if ( bFirstPersonArmsRescale ) {
		linearZ *= linearZ < 1.0 ? 10.0 : 1.0;
	}
	return linearZ * rcpFarZ;
}
void main() {
	{
		ivec3 pixelLocation = ivec3(gl_GlobalInvocationID );
		vec2 tc = ( pixelLocation.xy + 0.5 ) * _ca_freqHigh[0 ].zw;
		float ndcZ = tex2Dlod( samp_viewdepthmap, vec4( tc.xy, 0, 0) ).x;
		vec4 outputColor = vec4( 0, 0, 0, 1 );
		bool clearMask = false; if ( ndcZ < 1.0 ) {
			{
				{
					vec2 directional_occlusion = vec2( 1 ); if ( _ca_freqHigh[1 ].y > 0 ) {
						directional_occlusion = saturate( pow( tex2Dlod( samp_tex4, vec4( tc.xy, 0, 0 ) ).xx, _ca_freqHigh[1 ].zw ) );
					}
					vec3 specularBuffer = tex2Dlod( samp_tex0, vec4( tc.xy, 0, 0 ) ).xyz; if ( _ca_freqHigh[2 ].x > 0 ) {
						vec3 ssrBuffer = tex2Dlod( samp_tex2, vec4( tc.xy + _ca_freqHigh[0 ].zw*0.25, 0, 0 ) ).xyz;
						float ssrMask = tex2Dlod( samp_tex3, vec4( tc.xy, 0, 0 ) ).x;
						specularBuffer.xyz = mix( specularBuffer.xyz, ssrBuffer.xyz, ssrMask.xxx );
					}
					outputColor = vec4( specularBuffer.xyz * directional_occlusion.y, directional_occlusion.x );
				}
			};
		}
		else {
			clearMask = dot( tex2Dlod( samp_tex1, vec4( tc.xy, 0, 0 ) ), vec4( 1 ) ) == 0.0 ? false : true;
		}
		vec3 dstColor = imageLoad( ib_uavdeferredcomposite0, pixelLocation.xy ).xyz;
		dstColor.xyz = ( ndcZ == 1.0 && clearMask )? vec3( 0, 0, 0 ) : outputColor.xyz + dstColor.xyz * outputColor.w; if ( _ca_freqHigh[2 ].w == 1 ) {
			vec4 globalPosition;
			tc /= _ca_freqLow[0 ].xy;
			{
				vec4 windowPosition;
				windowPosition.xy = tc;
				windowPosition.z = ndcZ;
				windowPosition.w = 1.0;
				globalPosition.x = dot4( windowPosition, _ca_freqLow[1 ] );
				globalPosition.y = dot4( windowPosition, _ca_freqLow[2 ] );
				globalPosition.z = dot4( windowPosition, _ca_freqLow[3 ] );
				globalPosition.w = dot4( windowPosition, _ca_freqLow[4 ] );
				globalPosition.xyz /= globalPosition.w;
			};
			vec3 fogColor;
			float fogDensity;
			{
				{
					float FLT_EPSILON = 0.0001;
					float heightFogAmount = 0.0;
					vec3 heightFogColor = vec3( 0.0 ); if ( _ca_freqLow[5 ].z < 1.0 ) {
						vec3 cameraToPixel = globalPosition.xyz - _ca_freqLow[6 ].xyz;
						float distanceToPixel = length( cameraToPixel );
						float be = _ca_freqLow[5 ].x * exp2( - _ca_freqLow[7 ].w * ( _ca_freqLow[6 ].z - _ca_freqLow[7 ].x ) );
						float distanceInFog = max( distanceToPixel - _ca_freqLow[5 ].y, 0.0 );
						float effectiveZ = max( cameraToPixel.z, FLT_EPSILON ); if ( cameraToPixel.z < -FLT_EPSILON ) {
							effectiveZ = cameraToPixel.z;
						}
						float lineIntegral = be * distanceInFog;
						lineIntegral *= ( 1.0 - exp2( - _ca_freqLow[7 ].w * effectiveZ ) ) / ( _ca_freqLow[7 ].w * effectiveZ );
						float cosL = dot( _ca_freqLow[8 ].xyz, normalize( cameraToPixel ) );
						float blendValue = saturate( ( cosL - _ca_freqLow[7 ].y ) * _ca_freqLow[7 ].z );
						float inscatterAlpha = 1.0 - ( blendValue * blendValue );
						vec3 fogLightInscatterScaledColor = _ca_freqLow[9 ].xyz * _ca_freqLow[10 ].x;
						heightFogColor = mix( fogLightInscatterScaledColor, _ca_freqLow[11 ].xyz, inscatterAlpha );
						heightFogAmount = 1.0 - max( saturate( exp2( -lineIntegral ) ), _ca_freqLow[5 ].z );
					}
					float viewZ = GetLinearDepth( ndcZ, _ca_freqLow[12 ], 1, false );
					float fogDepth = clamp( viewZ - _ca_freqLow[13 ].x, 0.0, _ca_freqLow[13 ].y ) * _ca_freqLow[13 ].z;
					float distanceFog = 1.0 - saturate( exp2( -fogDepth ) );
					fogDensity = saturate( heightFogAmount + distanceFog );
					float fogSafeAmount = max( fogDensity, FLT_EPSILON );
					float fogSelection = max( 0.0, ( fogSafeAmount - distanceFog ) / fogSafeAmount );
					fogColor = mix( _ca_freqLow[14 ].xyz, heightFogColor, fogSelection );
				}
			};
			dstColor = mix( dstColor, fogColor * _ca_freqLow[15 ].x, saturate( fogDensity * _ca_freqLow[5 ].w ).xxx );
		}
		imageStore( ib_uavdeferredcomposite0, pixelLocation.xy, vec4( dstColor.xyz, 1 ) );
		imageStore( ib_uavdeferredcomposite1, pixelLocation.xy, vec4( dstColor.xyz, 1 ) );
	}
}