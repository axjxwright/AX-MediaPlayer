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
#include "cinder/audio/audio.h"
#include "AX-MediaPlayer.h"

#if __has_include( "cinder/CinderImGui.h")
    #include "cinder/CinderImGui.h"
     #define HAS_DEBUG_UI
    namespace ui = ImGui;
#endif

#undef HAS_DEBUG_UI

#define FORCE_NVIDIA_CARD_IF_PRESENT

#ifdef FORCE_NVIDIA_CARD_IF_PRESENT
extern "C" 
{
    __declspec(dllexport) int NvOptimusEnablement = 0x00000001; // This forces Intel GPU / Nvidia card selection
}
#endif

using namespace ci;
using namespace ci::app;

class SimplePlaybackApp : public app::App
{
public:
    void setup ( ) override;
    void update ( ) override;
    void draw ( ) override;
    void fileDrop ( FileDropEvent event ) override;
    void keyDown( KeyEvent event ) override;

protected:
    void connectSignals();
    void loadDefaultVideo();

    AX::Video::MediaPlayerRef     _player;
    AX::Video::MediaPlayer::Error _error{ AX::Video::MediaPlayer::Error::NoError };

    bool _hardwareAccelerated{ true };
    bool _approximateSeeking{ true };
    gl::TextureRef _texture;
};

void SimplePlaybackApp::setup ( )
{
#ifdef HAS_DEBUG_UI
    ui::Initialize ( );
#endif

  
    console() << gl::getString(GL_RENDERER) << std::endl;
    console() << gl::getString(GL_VERSION) << std::endl;
    
    loadDefaultVideo();
}

void SimplePlaybackApp::loadDefaultVideo()
{
    auto fmt = AX::Video::MediaPlayer::Format().HardwareAccelerated( _hardwareAccelerated );
    _player = AX::Video::MediaPlayer::Create( CINDER_PATH "/samples/QuickTimeBasic/assets/bbb.mp4", fmt );
    connectSignals();
    _player->Play();
}

void SimplePlaybackApp::keyDown( KeyEvent event )
{
    if( event.getChar() == 'r' )
    {
        loadDefaultVideo();
    }
}

void SimplePlaybackApp::fileDrop ( FileDropEvent event )
{
    _error = AX::Video::MediaPlayer::Error::NoError;

    auto fmt = AX::Video::MediaPlayer::Format ( ).HardwareAccelerated ( _hardwareAccelerated );
    _player = AX::Video::MediaPlayer::Create ( loadFile ( event.getFile ( 0 ) ), fmt );
    connectSignals();
    _player->Play ( );
}

void SimplePlaybackApp::connectSignals()
{
    _player->OnSeekStart.connect( [=] { std::cout << "OnSeekStart\n"; } );
    _player->OnSeekEnd.connect( [=] { std::cout << "OnSeekEnd\n"; } );
    _player->OnComplete.connect( [=] { std::cout << "OnComplete\n"; } );
    _player->OnReady.connect( [=] { std::cout << "OnReady: " << _player->GetDurationInSeconds() << std::endl; } );
    _player->OnError.connect( [=] ( AX::Video::MediaPlayer::Error error ) { _error = error; } );
    _player->OnBufferingStart.connect( [=] { std::cout << "OnBufferingStart\n"; } );
    _player->OnBufferingEnd.connect( [=] { std::cout << "OnBufferingEnd\n"; } );
}

void SimplePlaybackApp::update ( )
{
}

#ifdef HAS_DEBUG_UI
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
#endif

void SimplePlaybackApp::draw ( )
{
    gl::clear ( Colorf::black ( ) );

#ifdef HAS_DEBUG_UI
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
#endif

    if ( _player->CheckNewFrame ( ) )
    {
        // Only true if using the CPU render path
        if ( auto surface = _player->GetSurface ( ) )
        {
            _texture = *_player->GetTexture ( );
        }
    }

    if ( _player->IsHardwareAccelerated ( ) )
    {
        // You can still use this method in software rendered mode
        // but it will create a new texture every time it's called
        // even if ::CheckNewFrame() was false. Use the above method 
        // for optimal texture creation in software mode but if you 
        // don't care, this block is functionally identical in both paths
        if ( auto lease = _player->GetTexture ( ) )
        {
            // You can now use this texture until `lease` goes out
            // of scope (it will Unlock() the texture when destructing )
            gl::draw ( *lease, getWindowBounds ( ) );
        }
    }
    else
    {
        if ( _texture ) gl::draw ( _texture, getWindowBounds ( ) );
    }
}

void Init ( App::Settings * settings )
{
#ifdef CINDER_MSW
    settings->setConsoleWindowEnabled ( );
#endif
}

CINDER_APP ( SimplePlaybackApp, RendererGl ( RendererGl::Options() ), Init );
