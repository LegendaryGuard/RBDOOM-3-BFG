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

#include "precompiled.h"
#pragma hdrstop

#include "RmlShell.h"

#include "../Game_local.h"

#include "rmlui/RmlUserInterfaceLocal.h"

#include "RmlUi/Core/EventListener.h"
#include "RmlUi/Core/EventListenerInstancer.h"
#include "RmlUi/Core/TransformPrimitive.h"

class UI_Shell;
class MyEventListener : public Rml::EventListener
{
public:
	MyEventListener( UI_Shell* shell, const Rml::String& value, Rml::Element* element );

	void ProcessEvent( Rml::Event& event ) override;

	void OnDetach( Rml::Element* /*element*/ ) override;

private:
	UI_Shell* _shell;
	Rml::String value;
	Rml::Element* element;
};

class MyEventListenerInstancer : public Rml::EventListenerInstancer
{
public:
	MyEventListenerInstancer()
		: _shell( nullptr )
	{
	}

	void SetShell( UI_Shell* shell )
	{
		_shell = shell;
	}

	Rml::EventListener* InstanceEventListener( const Rml::String& value, Rml::Element* element ) override
	{
		return new MyEventListener( _shell, value, element );
	}

private:
	UI_Shell* _shell;
};

class EventHandler
{
public:
	virtual ~EventHandler() = default;
	virtual void ProcessEvent( Rml::Event& event, const Rml::String& value ) = 0;
};


MyEventListener::MyEventListener( UI_Shell* shell, const Rml::String& value, Rml::Element* element )
	: _shell( shell )
	, value( value )
	, element( element )
{
}

void MyEventListener::ProcessEvent( Rml::Event& event )
{
	using namespace Rml;

	idLexer src;
	idToken token;
	src.LoadMemory( value.c_str(), value.length(), "rmlCommands" );
	src.SetFlags( LEXFL_NOFATALERRORS | LEXFL_NOSTRINGCONCAT | LEXFL_NOSTRINGESCAPECHARS | LEXFL_ALLOWPATHNAMES );

	while( true )
	{
		if( !src.ReadToken( &token ) )
		{
			break;
		}

		if( !token.Icmp( ";" ) )
		{
			continue;
		}

		if( !token.Icmp( "exit" ) )
		{
			common->Quit();
		}

		if( !token.Icmp( "goto" ) )
		{
			src.ReadToken( &token );
			_shell->SetNextScreen( token.c_str() );
			event.GetTargetElement()->GetOwnerDocument()->Close();
		}

		if( !token.Icmp( "load" ) )
		{
			src.ReadToken( &token );
			_shell->SetNextScreen( token.c_str() );
		}

		if( !token.Icmp( "map" ) )
		{
			src.ReadToken( &token );
			cmdSystem->AppendCommandText( va( "map %s\n", token.c_str() ) );
		}

		if( !token.Icmp( "playsound" ) )
		{
			src.ReadToken( &token );
			_shell->PlaySound( token.c_str() );
		}

		if( !token.Icmp( "inhibitControl" ) )
		{
			src.ReadToken( &token );
			if( !token.Cmp( "=" ) )
			{
				src.ReadToken( &token );
			}
			if( !token.Icmp( "true" ) )
			{
				_shell->SetInhibitsControl( true );
			}
			else if( !token.Icmp( "false" ) )
			{
				_shell->SetInhibitsControl( false );
			}
		}

		if( !token.Icmp( "pauseGame" ) )
		{
			src.ReadToken( &token );
			if( !token.Cmp( "=" ) )
			{
				src.ReadToken( &token );
			}
			if( !token.Icmp( "true" ) )
			{
				_shell->SetIsPausingGame( true );
			}
			else if( !token.Icmp( "false" ) )
			{
				_shell->SetIsPausingGame( false );
			}
		}

		if( !token.Icmp( "enableCursor" ) )
		{
			src.ReadToken( &token );
			if( !token.Cmp( "=" ) )
			{
				src.ReadToken( &token );
			}
			if( !token.Icmp( "true" ) )
			{
				_shell->SetCursorEnabled( true );
			}
			else if( !token.Icmp( "false" ) )
			{
				_shell->SetCursorEnabled( false );
			}
		}
	}
}

void MyEventListener::OnDetach( Rml::Element* /*element*/ )
{
	delete this;
}

static MyEventListenerInstancer eventListenerInstancer;

// UI Code

UI_Shell::UI_Shell()
	: _eventListenerInstancer( &eventListenerInstancer )
	, _nextScreen( -1 )
	, _currentScreen( -1 )
	, _ui( nullptr )
	, _isActive( false )
	, _isPausingGame( false )
	, _inhibitsControl( false )
	, _showCursor( false )
{
	( ( MyEventListenerInstancer* )_eventListenerInstancer )->SetShell( this );
}

bool UI_Shell::Init( idSoundWorld* soundWorld )
{
	_ui = ( RmlUserInterfaceLocal* )rmlManager->Find( "main", true );
	_soundWorld = soundWorld;
	_inhibitsControl = true;
	_isActive = true;
	_isPausingGame = false;

	// Register event listeners. Note that registering event listeners must happen before documents are loaded.
	Rml::Factory::RegisterEventListenerInstancer( _eventListenerInstancer );

	SetNextScreen( "startmenu" );

	TransitionNextScreen();

	// When this object frees its resources after destructing, it frees itself using the overriden delete method.
	// Originally this was allocated with the non-overriden 'new' function. Annoying.
	Rml::StringList textureNames = Rml::GetTextureSourceList();

	for( const auto& texturePath : textureNames )
	{
		const idMaterial* material = declManager->FindMaterial( texturePath.c_str() );
		if( material )
		{
			material->ReloadImages( false );
		}
		else
		{
			common->Warning( "Failed to load rml texture %s", texturePath.c_str() );
		}
	}

	return true;
}

bool UI_Shell::IsActive()
{
	return _ui->Context() && _isActive;
}

bool UI_Shell::IsCursorEnabled() const
{
	return _ui->IsCursorEnabled();
}

void UI_Shell::SetCursorEnabled( bool showCursor )
{
	_ui->SetCursorEnabled( showCursor );
}

bool UI_Shell::HandleEvent( const sysEvent_t* event, int time )
{
	return _ui->HandleEvent( event, time );
}

void UI_Shell::Redraw( int time )
{
	if( !rmlManager->InLevelLoad() )
	{
		TransitionNextScreen();
	}

	_ui->Redraw( time );
}

struct ShellScreen
{
	idStr name;
	bool isInhibitingControl = false;
	bool isPausingGame = false;
	bool isCursorEnabled = true;
};

static ShellScreen screens[] =
{
	{"game", false, false, false},
	{"startmenu", true, false, true},
	{"options", true, false, true},
	{"pause", true, true, true}
};

void UI_Shell::SetNextScreen( const char* screen )
{
	if( _currentScreen >= 0 && !screens[_currentScreen].name.Icmp( screen ) )
	{
		return;
	}

	bool foundResult = false;

	for( int i = 0; i < sizeof( screens ) / sizeof( int ); i++ )
	{
		if( !screens[i].name.Icmp( screen ) )
		{
			_nextScreen = i;
			foundResult = true;
			break;
		}
	}

	if( !foundResult )
	{
		_nextScreen = -1;
	}
}

void UI_Shell::TransitionNextScreen()
{
	if( _nextScreen < 0 )
	{
		return;
	}

	LoadDocument( screens[_nextScreen].name.c_str() );
	_currentScreen = _nextScreen;
}

Rml::ElementDocument* UI_Shell::LoadDocument( const char* windowName )
{
	// Set the event handler for the new screen, if one has been registered.
//EventHandler* old_event_handler = event_handler;
//EventHandlerMap::iterator iterator = event_handlers.find(window_name);
//if (iterator != event_handlers.end())
//	event_handler = (*iterator).second;
//else
//	event_handler = nullptr;

	// Attempt to load the referenced RML document.
	idStrStatic<128> docPath = "guis/rml/shell";
	docPath.AppendPath( windowName );
	docPath.Append( ".rml" );

	Rml::ElementDocument* document = rmlManager->GetDocument( _ui->Context(), docPath.c_str() );

	idStr currentDocPath = "guis/rml/shell/";
	currentDocPath.AppendPath( screens[_currentScreen].name );
	currentDocPath.Append( ".rml" );

	if( currentDocPath != docPath )
	{
		rmlManager->CloseDocument( _ui->Context(), currentDocPath.c_str() );
	}

	if( document )
	{
		document->Show();
		return document;
	}

	// Reset these variables to default values.
	SetInhibitsControl( false );
	SetIsPausingGame( false );
	SetCursorEnabled( false );

	document = rmlManager->LoadDocument( _ui->Context(), docPath.c_str() );

	if( document == nullptr )
	{
		//event_handler = old_event_handler;
		common->Warning( "Failed to load document %s\n", docPath.c_str() );
		return nullptr;
	}

	if( !idStr::Icmp( windowName, "startmenu" ) )
	{
		using Rml::Transform;
		auto el = document->GetElementById( "start_game" );
		auto p1 = Transform::MakeProperty( { Rml::Transforms::Rotate2D{10.f}, Rml::Transforms::TranslateX{100.f} } );
		auto p2 = Transform::MakeProperty( { Rml::Transforms::Scale2D{3.f} } );
		el->Animate( "transform", p1, 1.8f, Rml::Tween{ Rml::Tween::Elastic, Rml::Tween::InOut }, -1, true );
		el->AddAnimationKey( "transform", p2, 1.3f, Rml::Tween{ Rml::Tween::Elastic, Rml::Tween::InOut } );
	}

	document->Show();

	return document;
}

int UI_Shell::PlaySound( const char* sound, int channel, bool blocking )
{
	if( !IsActive() )
	{
		return -1;
	}
	if( _soundWorld )
	{
		return _soundWorld->PlayShaderDirectly( sound, channel );
	}
	else
	{
		idLib::Warning( "No playing sound world on soundSystem in swf play sound!" );
		return -1;
	}
}

void UI_Shell::StopSound( int channel )
{
	if( _soundWorld )
	{
		_soundWorld->PlayShaderDirectly( NULL, channel );
	}
	else
	{
		idLib::Warning( "No playing sound world on soundSystem in rml play sound!" );
	}
}
