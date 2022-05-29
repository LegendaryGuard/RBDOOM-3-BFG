/*
===========================================================================

Doom 3 BFG Edition GPL Source Code
Copyright (C) 1993-2012 id Software LLC, a ZeniMax Media company.
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

#include <rmlui/RmlFontEngine.h>

#include "renderer/FontManager.h"
#include "renderer/RenderSystem.h"
#include "RmlUi/Core/FontEngineInterface.h"

RmlFontEngine::RmlFontEngine()
	: _fontManager( nullptr )
{
}

RmlFontEngine::~RmlFontEngine()
{
	RmlFontEngine::ReleaseFontResources();
}

void RmlFontEngine::Init()
{
	_fontManager = renderSystem->GetFontManager();
	const idMaterial* mat = renderSystem->GetTextBufferManager()->fontMaterial();
	_texture.Set( mat->GetName(), mat->GetName() );
}

bool RmlFontEngine::LoadFontFace( const Rml::String& file_name, bool fallback_face, Rml::Style::FontWeight weight )
{
	const TrueTypeHandle ttf = renderSystem->RegisterFontFace( file_name.c_str() );
	_fontFaces.AddUnique( ttf );
	return ttf.id != kInvalidHandle;
}

bool RmlFontEngine::LoadFontFace( const byte* data, int data_size, const Rml::String& family, Rml::Style::FontStyle style, Rml::Style::FontWeight weight, bool fallback_face )
{
	return false;
}

Rml::FontFaceHandle RmlFontEngine::GetFontFaceHandle( const Rml::String& family_, const Rml::Style::FontStyle style, Rml::Style::FontWeight weight, int size )
{
	FontStyle fontStyle = FONT_STYLE_NORMAL;
	if( style == Rml::Style::FontStyle::Italic )
	{
		fontStyle = FONT_STYLE_ITALIC;
	}

	const FontHandle handle = renderSystem->RegisterFont2( family_.c_str(), size, fontStyle );

	if( handle.id == kInvalidHandle )
	{
		return reinterpret_cast<Rml::FontFaceHandle>( nullptr );
	}

	_fonts.AddUnique( handle );
	return static_cast<Rml::FontFaceHandle>( handle.id ) + 1;
}

Rml::FontEffectsHandle RmlFontEngine::PrepareFontEffects( Rml::FontFaceHandle handle_, const Rml::FontEffectList& fontEffects_ )
{
	return 0;
}

int RmlFontEngine::GetSize( const Rml::FontFaceHandle handle_ )
{
	FontHandle fontHandle;
	fontHandle.id = static_cast<uint16_t>( handle_ ) - 1;
	return _fontManager->getFontInfo( fontHandle ).pixelSize;
}

int RmlFontEngine::GetXHeight( Rml::FontFaceHandle handle )
{
	const auto textBufferManager = renderSystem->GetTextBufferManager();
	const TextBufferHandle textHandle = textBufferManager->createTextBuffer( 1, BufferType::Transient );
	FontHandle fontHandle;
	fontHandle.id = static_cast<uint16_t>( handle ) - 1;
	textBufferManager->appendText( textHandle, fontHandle, "x" );
	const int height = textBufferManager->getRectangle( textHandle ).height;
	textBufferManager->destroyTextBuffer( textHandle );
	return height;
}

int RmlFontEngine::GetLineHeight( Rml::FontFaceHandle handle )
{
	FontHandle fontHandle;
	fontHandle.id = static_cast<uint16_t>( handle ) - 1;
	return _fontManager->getFontInfo( fontHandle ).pixelSize;
}

int RmlFontEngine::GetBaseline( Rml::FontFaceHandle handle )
{
	FontHandle fontHandle;
	fontHandle.id = static_cast<uint16_t>( handle ) - 1;
	return _fontManager->getFontInfo( fontHandle ).ascender;
}

float RmlFontEngine::GetUnderline( Rml::FontFaceHandle handle, float& thickness )
{
	FontHandle fontHandle;
	fontHandle.id = static_cast<uint16_t>( handle ) - 1;
	return _fontManager->getFontInfo( fontHandle ).underlinePosition;
}

int RmlFontEngine::GetStringWidth( Rml::FontFaceHandle handle, const Rml::String& string, Rml::Character prior_character )
{
	const auto textBufferManager = renderSystem->GetTextBufferManager();
	const TextBufferHandle textHandle = textBufferManager->createTextBuffer( 1, BufferType::Transient );
	FontHandle fontHandle;
	fontHandle.id = static_cast<uint16_t>( handle ) - 1;
	textBufferManager->appendText( textHandle, fontHandle, string.c_str() );
	const int width = textBufferManager->getRectangle( textHandle ).width;
	textBufferManager->destroyTextBuffer( textHandle );
	return width;
}

int RmlFontEngine::GenerateString( Rml::FontFaceHandle faceHandle_, Rml::FontEffectsHandle fontEffectsHandle_, const Rml::String& string_, const Rml::Vector2f& position_, const Rml::Colourb& colour_, float opacity_, Rml::GeometryList& geometry_ )
{
	const auto textBufferManager = renderSystem->GetTextBufferManager();
	const TextBufferHandle textHandle = textBufferManager->createTextBuffer( 1, BufferType::Transient );
	FontHandle fontHandle;
	fontHandle.id = ( uint16_t )faceHandle_ - 1;
	textBufferManager->setPenPosition( textHandle, position_.x, position_.y );
	const idVec4 theColor( colour_.red, colour_.green, colour_.blue, ( opacity_ * colour_.alpha ) );
	textBufferManager->setTextColor( textHandle, VectorUtil::Vec4ToColorInt( theColor / 255.f ) );
	textBufferManager->appendText( textHandle, fontHandle, string_.c_str() );
	const int width = textBufferManager->getRectangle( textHandle ).width;
	const idDrawVert* verts = textBufferManager->getVertices( textHandle );
	const uint16_t* indexes = textBufferManager->getIndexes( textHandle );
	const int vertexCount = textBufferManager->vertexCount( textHandle );
	const int indexCount = textBufferManager->indexCount( textHandle );

	geometry_.emplace_back();
	Rml::Geometry& geo = geometry_.back();

	geo.GetVertices().reserve( vertexCount );

	for( int i = 0; i < vertexCount; i++ )
	{
		auto& vert = verts[ i ].xyz;
		Rml::Vertex vertex;
		vertex.position.x = vert.x;
		vertex.position.y = vert.y;
		idVec4 color;
		UnpackColor( verts[ i ].GetColor(), color );
		vertex.colour = Rml::Colourb( idMath::Ftob( color.x * 255.f ),
									  idMath::Ftob( color.y * 255.f ),
									  idMath::Ftob( color.z * 255.f ),
									  idMath::Ftob( color.w * 255.f ) );
		vertex.tex_coord.x = verts[ i ].GetTexCoordS();
		vertex.tex_coord.y = verts[ i ].GetTexCoordT();
		geo.GetVertices().push_back( vertex );
	}

	geo.GetIndices().reserve( indexCount );

	for( int i = 0; i < indexCount; i++ )
	{
		geo.GetIndices().emplace_back( indexes[ i ] );
	}

	geo.SetTexture( &_texture );

	textBufferManager->destroyTextBuffer( textHandle );
	return width;
}

int RmlFontEngine::GetVersion( Rml::FontFaceHandle handle_ )
{
	return 0;
}

void RmlFontEngine::ReleaseFontResources()
{
	for( const auto font : _fonts )
	{
		renderSystem->FreeFont( font );
	}
	_fonts.Clear();

	for( const auto fontFace : _fontFaces )
	{
		renderSystem->FreeFontFace( fontFace );
	}
	_fontFaces.Clear();
}