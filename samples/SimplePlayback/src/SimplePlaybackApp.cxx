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
    void fileDrop ( FileDropEvent event ) override;

protected:

    AX::Video::MediaPlayerRef     _player;
    AX::Video::MediaPlayer::Error _error{ AX::Video::MediaPlayer::Error::NoError };

    bool _hardwareAccelerated{ true };
    bool _approximateSeeking{ true };
    gl::TextureRef _texture;
};

void SimplePlaybackApp::setup ( )
{
    ui::Initialize ( );

    uint32_t flags = 0;
    if ( _hardwareAccelerated ) flags |= AX::Video::MediaPlayer::HardwareAccelerated;
    
    //_player = AX::Video::MediaPlayer::Create ( loadFile ( CINDER_PATH "\\samples\\QuickTimeBasic\\assets\\bbb.mp4" ), flags );
    _player = AX::Video::MediaPlayer::Create ( loadFile ( "C:\\Dev\\Experiments\\8KVideo.mp4" ), flags );
    _player->OnSeekStart.connect ( [=] { std::cout << "OnSeekStart\n"; } );
    _player->OnSeekEnd.connect ( [=] { std::cout << "OnSeekEnd\n"; } );
    _player->OnComplete.connect ( [=] { std::cout << "OnComplete\n"; } );
    _player->OnError.connect ( [=] ( AX::Video::MediaPlayer::Error error ) { _error = error; } );
    _player->Play ( );
}

void SimplePlaybackApp::fileDrop ( FileDropEvent event )
{
    uint32_t flags = 0;
    if ( _hardwareAccelerated ) flags |= AX::Video::MediaPlayer::HardwareAccelerated;

    _error = AX::Video::MediaPlayer::Error::NoError;

    _player = AX::Video::MediaPlayer::Create ( loadFile ( event.getFile ( 0 ) ), flags );
    _player->OnSeekStart.connect ( [=] { std::cout << "OnSeekStart\n"; } );
    _player->OnSeekEnd.connect ( [=] { std::cout << "OnSeekEnd\n"; } );
    _player->OnComplete.connect ( [=] { std::cout << "OnComplete\n"; } );
    _player->OnError.connect ( [=] ( AX::Video::MediaPlayer::Error error ) { _error = error; } );
    _player->Play ( );
}

void SimplePlaybackApp::update ( )
{
}

struct ScopedWindow2
{
    ScopedWindow2 ( const char * title, uint32_t flags )
    {
        ui::Begin ( title, nullptr, flags );
    }

    ~ScopedWindow2 ( )
    {
        ui::End ( );
    }
};

void SimplePlaybackApp::draw ( )
{
    gl::clear ( Colorf::black ( ) );

    {
        ScopedWindow2 window{ "Settings", ImGuiWindowFlags_AlwaysAutoResize };

        ui::Checkbox ( "Use H/W Acceleration", &_hardwareAccelerated );
        if ( _error != AX::Video::MediaPlayer::Error::NoError )
        {
            ui::TextColored ( ImVec4 ( 0.8f, 0.1f, 0.1f, 1.0f ), "Error: %s", AX::Video::MediaPlayer::ErrorToString ( _error ).c_str ( ) );
            return;
        }

        if ( !_player ) return;

        float position = _player->GetPositionInSeconds ( );
        float duration = _player->GetDurationInSeconds ( );
        float percent = position / duration;

        ui::Text ( "%.2f FPS", getAverageFps ( ) );
        ui::Text ( "Hardware Accelerated: %s, HasAudio: %s, HasVideo : %s", _player->IsHardwareAccelerated() ? "true" : "false", _player->HasAudio ( ) ? "true" : "false", _player->HasVideo ( ) ? "true" : "false" );

        ui::Checkbox ( "Approximate Seeking", &_approximateSeeking );
        if ( ui::SliderFloat ( "Seek", &percent, 0.0f, 1.0f ) )
        {
            _player->SeekToPercentage ( percent, _approximateSeeking );
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

    if ( _player->CheckNewFrame ( ) )
    {
        // Only true if using the CPU render path
        if ( auto surface = _player->GetSurface ( ) )
        {
            _texture = *_player->GetTexture ( );
        }
    }

    // Only true if using DXGI path
    if ( auto lease = _player->GetTexture ( ) )
    {
        // You can now use this texture until `lease` goes out
        // of scope (it will Unlock() the texture when destructing )
        gl::draw ( *lease, getWindowBounds ( ) );
    }
    else
    {
        if ( _texture ) gl::draw ( _texture, getWindowBounds ( ) );
    }
}

void Init ( App::Settings * settings )
{
    settings->setConsoleWindowEnabled ( );
}

CINDER_APP ( SimplePlaybackApp, RendererGl ( RendererGl::Options() ), Init );