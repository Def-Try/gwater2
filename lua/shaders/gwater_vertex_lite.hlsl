#include "common_vs_fxc.h"

struct VS_INPUT
{
	float4 vPos					: POSITION;		// Position
	float4 vNormal				: NORMAL;		// Normal
	float4 vBoneWeights			: BLENDWEIGHT;	// Skin weights
	float4 vBoneIndices			: BLENDINDICES;	// Skin indices
	float4 vTexCoord			: TEXCOORD0;	// Texture coordinates
};

struct VS_OUTPUT
{
	float4 projPosSetup	: POSITION;
	float4 world_coord	: TEXCOORD0;
};

VS_OUTPUT main( const VS_INPUT v )
{
	VS_OUTPUT o = (VS_OUTPUT)0;

	//float3 vNormal;
	//DecompressVertex_Normal( v.vNormal, vNormal );

	float3 worldNormal, worldPos;
	SkinPositionAndNormal( SKINNING, v.vPos, v.vNormal, v.vBoneWeights, v.vBoneIndices, worldPos, worldNormal );

	float4 vProjPos = mul( float4( worldPos, 1 ), cViewProj );
	vProjPos.z = 0;//dot( float4( worldPos, 1  ), cViewProjZ );
	o.projPosSetup = vProjPos;
	o.world_coord = v.vTexCoord;
	return o;
};