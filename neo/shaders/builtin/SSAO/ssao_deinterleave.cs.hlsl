/*
* Copyright (c) 2014-2021, NVIDIA CORPORATION. All rights reserved.
*
* Permission is hereby granted, free of charge, to any person obtaining a
* copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation
* the rights to use, copy, modify, merge, publish, distribute, sublicense,
* and/or sell copies of the Software, and to permit persons to whom the
* Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in
* all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
* THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
* FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
* DEALINGS IN THE SOFTWARE.
*/

#pragma pack_matrix(row_major)

#include <global_inc.hlsl>

struct SsaoConstants
{
	float2      clipToView;
	float2      invQuantizedGbufferSize;

	int2        quantizedViewportOrigin;
	float       amount;
	float       invBackgroundViewDepth;
	float       radiusWorld;
	float       surfaceBias;

	float       radiusToScreen;
	float       powerExponent;
};

cbuffer c_Ssao :
register( b1 )
{
	SsaoConstants g_Ssao;
};

Texture2D<float> t_InputDepth :
register( t0 );
RWTexture2DArray<float> u_DeinterleavedDepth :
register( u0 );

[numthreads( 8, 8, 1 )]
void main( uint3 globalId : SV_DispatchThreadID )
{
	float depths[16];
	uint2 groupBase = globalId.xy * 4 + g_Ssao.quantizedViewportOrigin;

	[unroll]
	for( uint y = 0; y < 4; y++ )
	{
		[unroll]
		for( uint x = 0; x < 4; x++ )
		{
			uint2 gbufferSamplePos = groupBase + uint2( x, y );
			float depth = t_InputDepth[gbufferSamplePos];

#if LINEAR_DEPTH
			float linearDepth = depth;
#else
			float4 clipPos = float4( 0, 0, depth, 1 );
			float4 viewPos;
			viewPos.x = dot4( clipPos, rpModelMatrixX );
			viewPos.y = dot4( clipPos, rpModelMatrixY );
			viewPos.z = dot4( clipPos, rpModelMatrixZ );
			viewPos.w = dot4( clipPos, rpModelMatrixW );

			float linearDepth = viewPos.z / viewPos.w;
#endif

			depths[y * 4 + x] = linearDepth;
		}
	}

	uint2 quarterResPos = groupBase >> 2;

	[unroll]
	for( uint index = 0; index < 16; index++ )
	{
		float depth = depths[index];
		u_DeinterleavedDepth[uint3( quarterResPos.xy, index )] = depth;
	}
}
