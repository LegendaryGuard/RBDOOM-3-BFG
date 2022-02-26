/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company.

This file is part of the Doom 3 BFG Edition GPL Source Code ("Doom 3 BFG Edition Source Code").

Doom 3 BFG Edition Source Code is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

Doom 3 BFG Edition Source Code is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with Doom 3 BFG Edition Source Code.  If not, see <http://www.gnu.org/licenses/>.

In addition, the Doom 3 BFG Edition Source Code is also subject to certain additional terms. You should have received a copy of these additional terms immediately following the terms and conditions of the GNU General Public License which accompanied the Doom 3 BFG Edition Source Code.  If not, please request a copy in writing from id Software at the address below.

If you have questions concerning this license or the applicable additional terms, you may contact in writing id Software LLC, c/o ZeniMax Media Inc., Suite 120, Rockville, Maryland 20850 USA.

===========================================================================
*/

#include "global_inc.hlsl"


// *INDENT-OFF*
Texture2D t_CurrentRender : register( t0 );
Texture2D t_NormalMap : register( t1 );

SamplerState LinearSampler : register( s0 );

struct PS_IN {
	float4 position		: SV_Position;
	float2 texcoord		: TEXCOORD0_centroid;
	float4 color		: COLOR0;
};

struct PS_OUT {
	float4 color : SV_Target;
};
// *INDENT-ON*

void main( PS_IN fragment, out PS_OUT result )
{

	float2 screenTexCoord = fragment.texcoord;

	// compute warp factor
	float4 warpFactor = 1.0 - ( t_NormalMap.Sample( LinearSampler, screenTexCoord.xy ) * fragment.color );
	screenTexCoord -= float2( 0.5, 0.5 );
	screenTexCoord *= warpFactor.xy;
	screenTexCoord += float2( 0.5, 0.5 );

	// load the screen render
	result.color = t_CurrentRender.Sample( LinearSampler, screenTexCoord );

}