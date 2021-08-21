//
//  SimplePlaybackApp.cxx
//  SimplePlaybackApp
//
//  Created by Andrew Wright on 17/08/21.
//  (c) 2021 AX Interactive
//

#include "cinder/app/RendererGl.h"
#include "cinder/app/App.h"
#include "cinder/gl/gl.h"
#include "cinder/CinderImGui.h"
#include "AX-MediaPlayer.h"

using namespace ci;
using namespace ci::app;
namespace ui = ImGui;

class SimplePlaybackApp : public app::App
{
public:

    void setup ( ) override;
    void update ( ) override;
    void draw ( ) override;

protected:

    AX::Video::MediaPlayerRef    _player;
    AX::Video::MediaPlayer::Error _error{ AX::Video::MediaPlayer::Error::NoError };

    gl::TextureRef _texture;
};

void SimplePlaybackApp::setup ( )
{
    ui::Initialize ( );

    _player = AX::Video::MediaPlayer::Create ( loadFile ( CINDER_PATH "\\samples\\QuickTimeBasic\\assets\\bbb.mp4" ), AX::Video::MediaPlayer::HardwareAccelerated );
    _player->OnFrameReady.connect ( [=]
    {
        if ( auto surf = _player->GetCurrentSurface ( ) )
        {
            _texture = gl::Texture::create ( *surf );
        }
    } );
     
    _player->OnSeekStart.connect ( [=] { std::cout << "OnSeekStart\n"; } );
    _player->OnSeekEnd.connect ( [=] { std::cout << "OnSeekEnd\n"; } );
    _player->OnComplete.connect ( [=] { std::cout << "OnComplete\n"; } );
    _player->OnError.connect ( [=] ( AX::Video::MediaPlayer::Error error ) { _error = error; } );

    _player->Play ( );
}

void SimplePlaybackApp::update ( )
{
}

void SimplePlaybackApp::draw ( )
{
    if ( _texture ) gl::draw ( _texture, getWindowBounds() );

    {
        ui::ScopedWindow window{ "Settings" };

        if ( _error != AX::Video::MediaPlayer::Error::NoError )
        {
            ui::TextColored ( ImVec4 ( 0.8f, 0.1f, 0.1f, 1.0f ), "Error: %s", AX::Video::MediaPlayer::ErrorToString ( _error ).c_str ( ) );
            return;
        }

        float position = _player->GetPositionInSeconds ( );
        float duration = _player->GetDurationInSeconds ( );
        float percent = position / duration;

        ui::Text ( "%.2f FPS / HasAudio: %s, HasVideo: %s", getAverageFps ( ), _player->HasAudio ( ) ? "true" : "false", _player->HasVideo ( ) ? "true" : "false" );

        if ( ui::SliderFloat ( "Seek", &percent, 0.0f, 1.0f ) )
        {
            _player->SeekToPercentage ( percent );
        }

        float rate = _player->GetPlaybackRate ( );
        if ( ui::SliderFloat ( "Playback Rate", &rate, -2.5f, 2.5f ) )
        {
            _player->SetPlaybackRate ( rate );
        }

        if ( _player->IsPaused ( ) )
        {
            if ( ui::Button ( "Play" ) )
            {
                _player->Play ( );
            }
        }
        else if ( _player->IsPlaying ( ) )
        {
            if ( ui::Button ( "Pause" ) )
            {
                _player->Pause ( );
            }
        }

        ui::SameLine ( );
        if ( ui::Button ( "Toggle" ) ) _player->TogglePlayback ( );
        
        ui::SameLine ( );
        bool loop = _player->IsLooping ( );
        if ( ui::Checkbox ( "Loop", &loop ) ) _player->SetLoop ( loop );

        ui::SameLine ( );
        bool mute = _player->IsMuted ( );
        if ( ui::Checkbox ( "Mute", &mute ) ) _player->SetMuted ( mute );

        if ( !mute )
        {
            float volume = _player->GetVolume ( );
            if ( ui::DragFloat ( "Volume", &volume, 0.01f, 0.0f, 1.0f ) )
            {
                _player->SetVolume ( volume );
            }
        }
    }
}

void Init ( App::Settings * settings )
{
    settings->setConsoleWindowEnabled ( );
}

CINDER_APP ( SimplePlaybackApp, RendererGl, Init );