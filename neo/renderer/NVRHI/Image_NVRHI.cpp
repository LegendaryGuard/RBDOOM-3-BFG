/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company.
Copyright (C) 2013-2022 Robert Beckebans
Copyright (C) 2022 Stephen Pridham

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
#include "precompiled.h"
#pragma hdrstop

/*
================================================================================================
Contains the Image implementation for OpenGL.
================================================================================================
*/

#include "../RenderCommon.h"

#include "sys/DeviceManager.h"

extern DeviceManager* deviceManager;

/*
====================
idImage::idImage
====================
*/
idImage::idImage( const char* name ) : imgName( name )
{
	texture.Reset();
	generatorFunction = NULL;
	filter = TF_DEFAULT;
	repeat = TR_REPEAT;
	usage = TD_DEFAULT;
	cubeFiles = CF_2D;
	cubeMapSize = 0;
	isLoaded = false;

	referencedOutsideLevelLoad = false;
	levelLoadReferenced = false;
	defaulted = false;
	sourceFileTime = FILE_NOT_FOUND_TIMESTAMP;
	binaryFileTime = FILE_NOT_FOUND_TIMESTAMP;
	refCount = 0;

	DeferredLoadImage();
}

/*
====================
idImage::~idImage
====================
*/
idImage::~idImage()
{
	PurgeImage();
}

/*
====================
idImage::IsLoaded
====================
*/
bool idImage::IsLoaded() const
{
	return isLoaded;
}

void idImage::CreateSampler()
{
	sampler.Reset();

	samplerDesc = nvrhi::SamplerDesc()
				  .setAllFilters( false )
				  .setAllAddressModes( nvrhi::SamplerAddressMode::Clamp )
				  .setMaxAnisotropy( 1.0f );

	if( opts.format == FMT_DEPTH || opts.format == FMT_DEPTH_STENCIL )
	{
		samplerDesc.setReductionType( nvrhi::SamplerReductionType::Comparison );
	}

	switch( filter )
	{
		case TF_DEFAULT:
			samplerDesc.setAllFilters( true )
			.setMaxAnisotropy( r_maxAnisotropicFiltering.GetInteger() );
			break;

		case TF_LINEAR:
			samplerDesc.setAllFilters( true );
			break;

		case TF_NEAREST:
			samplerDesc.setAllFilters( false );
			break;

		// RB:
		case TF_NEAREST_MIPMAP:
			samplerDesc.setMipFilter( true );
			break;

		default:
			idLib::FatalError( "idImage::CreateSampler: unrecognized texture filter %d", filter );
	}

	switch( repeat )
	{
		case TR_REPEAT:
			samplerDesc.setAddressU( nvrhi::SamplerAddressMode::Repeat )
			.setAddressV( nvrhi::SamplerAddressMode::Repeat )
			.setAddressW( nvrhi::SamplerAddressMode::Repeat );
			break;

		case TR_CLAMP:
			samplerDesc.setAddressU( nvrhi::SamplerAddressMode::ClampToEdge )
			.setAddressV( nvrhi::SamplerAddressMode::ClampToEdge )
			.setAddressW( nvrhi::SamplerAddressMode::ClampToEdge );
			break;

		case TR_CLAMP_TO_ZERO_ALPHA:
			samplerDesc.setBorderColor( nvrhi::Color( 0.f, 0.f, 0.f, 0.f ) )
			.setAddressU( nvrhi::SamplerAddressMode::ClampToBorder )
			.setAddressV( nvrhi::SamplerAddressMode::ClampToBorder )
			.setAddressW( nvrhi::SamplerAddressMode::ClampToBorder );
			break;

		case TR_CLAMP_TO_ZERO:
			samplerDesc.setBorderColor( nvrhi::Color( 0.f, 0.f, 0.f, 1.f ) )
			.setAddressU( nvrhi::SamplerAddressMode::ClampToBorder )
			.setAddressV( nvrhi::SamplerAddressMode::ClampToBorder )
			.setAddressW( nvrhi::SamplerAddressMode::ClampToBorder );
			break;
		default:
			idLib::FatalError( "idImage::CreateSampler: unrecognized texture repeat mode %d", repeat );
	}

	//sampler = deviceManager->GetDevice()->createSampler( samplerDesc );
}

/*
==============
Bind

Automatically enables 2D mapping or cube mapping if needed
==============
*/
void idImage::Bind()
{
	tr.backend.SetCurrentImage( this );
}

/*
====================
CopyFramebuffer
====================
*/
void idImage::CopyFramebuffer( int x, int y, int imageWidth, int imageHeight )
{
	tr.backend.pc.c_copyFrameBuffer++;
}

/*
====================
CopyDepthbuffer
====================
*/
void idImage::CopyDepthbuffer( int x, int y, int imageWidth, int imageHeight )
{
	tr.backend.pc.c_copyFrameBuffer++;
}

/*
========================
idImage::SubImageUpload
========================
*/
void idImage::SubImageUpload( int mipLevel, int x, int y, int z, int width, int height, const void* pic, nvrhi::ICommandList* commandList, int pixelPitch )
{
	assert( x >= 0 && y >= 0 && mipLevel >= 0 && width >= 0 && height >= 0 && mipLevel < opts.numLevels );
}

/*
========================
idImage::SetSamplerState
========================
*/
void idImage::SetSamplerState( textureFilter_t tf, textureRepeat_t tr )
{
}

/*
========================
idImage::SetTexParameters
========================
*/
void idImage::SetTexParameters()
{
}

/*
========================
idImage::AllocImage

Every image will pass through this function. Allocates all the necessary MipMap levels for the
Image, but doesn't put anything in them.

This should not be done during normal game-play, if you can avoid it.
========================
*/
void idImage::AllocImage()
{
	PurgeImage();

	nvrhi::Format format = nvrhi::Format::RGBA8_UINT;
	int bpp = 4;

	CreateSampler();

	switch( opts.format )
	{
		case FMT_RGBA8:
			format = nvrhi::Format::RGBA8_UNORM;
			break;

		case FMT_XRGB8:
			format = nvrhi::Format::X32G8_UINT;
			break;

		case FMT_RGB565:
			format = nvrhi::Format::B5G6R5_UNORM;
			break;

		case FMT_ALPHA:
			format = nvrhi::Format::R8_UNORM;
			break;

		case FMT_L8A8:
			format = nvrhi::Format::RG8_UNORM;
			break;

		case FMT_LUM8:
			format = nvrhi::Format::R8_UNORM;
			break;

		case FMT_INT8:
			format = nvrhi::Format::R8_UNORM;
			break;

		case FMT_R8:
			format = nvrhi::Format::R8_UNORM;
			break;

		case FMT_DXT1:
			format = nvrhi::Format::BC1_UNORM;
			break;

		case FMT_DXT5:
			format = nvrhi::Format::BC3_UNORM;
			break;

		case FMT_DEPTH:
			format = nvrhi::Format::D32;
			break;

		case FMT_DEPTH_STENCIL:
			format = nvrhi::Format::D24S8;
			break;

		case FMT_SHADOW_ARRAY:
			format = nvrhi::Format::D32;
			break;

		case FMT_RG16F:
			format = nvrhi::Format::RG16_FLOAT;
			break;

		case FMT_RGBA16F:
			format = nvrhi::Format::RGBA16_FLOAT;
			break;

		case FMT_RGBA16S:
			format = nvrhi::Format::RGBA16_SNORM;
			break;

		case FMT_RGBA32F:
			format = nvrhi::Format::RGBA32_FLOAT;
			break;

		case FMT_R32F:
			format = nvrhi::Format::R32_FLOAT;
			break;

		case FMT_X16:
			//internalFormat = GL_INTENSITY16;
			//dataFormat = GL_LUMINANCE;
			//dataType = GL_UNSIGNED_SHORT;
			//format = nvrhi::Format::Lum
			break;
		case FMT_Y16_X16:
			//internalFormat = GL_LUMINANCE16_ALPHA16;
			//dataFormat = GL_LUMINANCE_ALPHA;
			//dataType = GL_UNSIGNED_SHORT;
			break;

		// see http://what-when-how.com/Tutorial/topic-615ll9ug/Praise-for-OpenGL-ES-30-Programming-Guide-291.html
		case FMT_R11G11B10F:
			format = nvrhi::Format::R11G11B10_FLOAT;
			break;

		case FMT_SRGB8:
			format = nvrhi::Format::SRGBA8_UNORM;
			break;

		default:
			idLib::Error( "Unhandled image format %d in %s\n", opts.format, GetName() );
	}

	// if we don't have a rendering context, just return after we
	// have filled in the parms.  We must have the values set, or
	// an image match from a shader before OpenGL starts would miss
	// the generated texture
	if( !tr.IsInitialized() )
	{
		return;
	}

	uint originalWidth = opts.width;
	uint originalHeight = opts.height;

	if( IsCompressed() )
	{
		originalWidth = ( originalWidth + 3 ) & ~3;
		originalHeight = ( originalHeight + 3 ) & ~3;
	}

	uint scaledWidth = originalWidth;
	uint scaledHeight = originalHeight;

#if 0
	uint maxTextureSize = 0;

	if( maxTextureSize > 0 &&
			int( std::max( originalWidth, originalHeight ) ) > maxTextureSize &&
			opts.isRenderTarget &&
			opts.textureType == TT_2D )
	{
		if( originalWidth >= originalHeight )
		{
			scaledHeight = originalHeight * maxTextureSize / originalWidth;
			scaledWidth = maxTextureSize;
		}
		else
		{
			scaledWidth = originalWidth * maxTextureSize / originalHeight;
			scaledHeight = maxTextureSize;
		}
	}
#endif

	auto textureDesc = nvrhi::TextureDesc()
					   .setDebugName( GetName() )
					   .setDimension( nvrhi::TextureDimension::Texture2D )
					   .setWidth( scaledWidth )
					   .setHeight( scaledHeight )
					   .setFormat( format )
					   .setIsUAV( opts.isUAV )
					   .setSampleCount( opts.samples )
					   .setMipLevels( opts.numLevels );

	if( opts.colorFormat == CFM_GREEN_ALPHA )
	{
		textureDesc.componentMapping.r = nvrhi::ComponentSwizzle::One;
		textureDesc.componentMapping.g = nvrhi::ComponentSwizzle::One;
		textureDesc.componentMapping.b = nvrhi::ComponentSwizzle::One;
		textureDesc.componentMapping.a = nvrhi::ComponentSwizzle::Green;
	}
	else if( opts.format == FMT_LUM8 )
	{
		textureDesc.componentMapping.r = nvrhi::ComponentSwizzle::Red;
		textureDesc.componentMapping.g = nvrhi::ComponentSwizzle::Red;
		textureDesc.componentMapping.b = nvrhi::ComponentSwizzle::Red;
		textureDesc.componentMapping.a = nvrhi::ComponentSwizzle::One;
	}
	else if( opts.format == FMT_L8A8 )
	{
		textureDesc.componentMapping.r = nvrhi::ComponentSwizzle::Red;
		textureDesc.componentMapping.g = nvrhi::ComponentSwizzle::Red;
		textureDesc.componentMapping.b = nvrhi::ComponentSwizzle::Red;
		textureDesc.componentMapping.a = nvrhi::ComponentSwizzle::Green;
	}
	else if( opts.format == FMT_ALPHA )
	{
		textureDesc.componentMapping.r = nvrhi::ComponentSwizzle::One;
		textureDesc.componentMapping.g = nvrhi::ComponentSwizzle::One;
		textureDesc.componentMapping.b = nvrhi::ComponentSwizzle::One;
		textureDesc.componentMapping.a = nvrhi::ComponentSwizzle::Red;
	}
	else if( opts.format == FMT_INT8 )
	{
		textureDesc.componentMapping.r = nvrhi::ComponentSwizzle::Red;
		textureDesc.componentMapping.g = nvrhi::ComponentSwizzle::Red;
		textureDesc.componentMapping.b = nvrhi::ComponentSwizzle::Red;
		textureDesc.componentMapping.a = nvrhi::ComponentSwizzle::Red;
	}
	else if( opts.format == FMT_R11G11B10F )
	{
		textureDesc.componentMapping.r = nvrhi::ComponentSwizzle::Red;
		textureDesc.componentMapping.g = nvrhi::ComponentSwizzle::Green;
		textureDesc.componentMapping.b = nvrhi::ComponentSwizzle::Blue;
		textureDesc.componentMapping.a = nvrhi::ComponentSwizzle::One;
	}

	if( opts.isRenderTarget )
	{
		textureDesc.setInitialState( nvrhi::ResourceStates::RenderTarget )
				   .setClearValue( nvrhi::Color( 0.f ) )
				   .setIsRenderTarget( true )
				   .setKeepInitialState( true );

		if( opts.format == FMT_DEPTH || opts.format == FMT_DEPTH_STENCIL || opts.format == FMT_SHADOW_ARRAY )
		{
			textureDesc.setInitialState( nvrhi::ResourceStates::DepthWrite )
					   .setClearValue( nvrhi::Color( 1.f ) );
		}

		if( opts.isUAV )
		{
			// TODO(Stephen): Probably make this an image option.
			// This is a hack to make cszBuffer and ambient occlusion uav work.
			textureDesc.setIsUAV( true );
		}
	}

	if( opts.textureType == TT_2D )
	{
		textureDesc.setDimension( nvrhi::TextureDimension::Texture2D );
	}
	else if( opts.textureType == TT_CUBIC )
	{
		textureDesc.setDimension( nvrhi::TextureDimension::TextureCube );
		textureDesc.setArraySize( 6 );
	}
	else if( opts.textureType == TT_2D_ARRAY )
	{
		textureDesc.setDimension( nvrhi::TextureDimension::Texture2DArray );
		textureDesc.setArraySize( 6 );
	}
	else if( opts.textureType == TT_2D_MULTISAMPLE )
	{
		textureDesc.setDimension( nvrhi::TextureDimension::Texture2DMS );
		textureDesc.setArraySize( 1 );
	}

	texture = deviceManager->GetDevice()->createTexture( textureDesc );

	assert( texture );
}

/*
========================
idImage::PurgeImage
========================
*/
void idImage::PurgeImage()
{
	texture.Reset();
	sampler.Reset();
	isLoaded = false;
	defaulted = false;
}

/*
========================
idImage::Resize
========================
*/
void idImage::Resize( int width, int height )
{
	if( opts.width == width && opts.height == height )
	{
		return;
	}
	opts.width = width;
	opts.height = height;
	AllocImage();
}
