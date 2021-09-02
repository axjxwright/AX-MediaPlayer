//
//  AX-MediaPlayer.cxx
//  AX-MediaPlayer
//
//  Created by Andrew Wright (@axjxwright) on 17/08/21.
//  (c) 2021 AX Interactive (axinteractive.com.au)
//  
//

#include "AX-MediaPlayer.h"
#include "cinder/app/App.h"
#include "cinder/audio/Device.h"

#include <cstdint>
#include <iostream>

#ifdef WIN32
    #include "Win32/AX-MediaPlayerWin32Impl.h"
#else
    #error "Unsupported Platform"
#endif

using namespace ci;

namespace AX
{
    namespace Video
    {
        const std::string & MediaPlayer::ErrorToString ( MediaPlayer::Error error )
        {
            static std::unordered_map<MediaPlayer::Error, std::string> kErrors =
            {
                {  MediaPlayer::Error::NoError, "No Error" },
                {  MediaPlayer::Error::Aborted, "Aborted" },
                {  MediaPlayer::Error::NetworkError, "Network Error" },
                {  MediaPlayer::Error::DecodingError, "Decoding Error" },
                {  MediaPlayer::Error::SourceNotSupported, "Source Not Supported" },
                {  MediaPlayer::Error::Encrypted, "Source Is Encrypted" },
            };

            if ( kErrors.count ( error ) )
            {
                return kErrors.at ( error );
            }
            else
            {
                static std::string kUnknownError = "Unknown Error";
                return kUnknownError;
            }
        }

        MediaPlayer::Format & MediaPlayer::Format::AudioDevice ( const ci::audio::DeviceRef & device )
        {
            if ( device ) _audioDeviceId = device->getKey ( );
            return *this;
        }

        MediaPlayerRef MediaPlayer::Create ( const ci::DataSourceRef & source, const MediaPlayer::Format& fmt )
        {
            return MediaPlayerRef ( new MediaPlayer ( source, fmt ) );
        }

        MediaPlayerRef MediaPlayer::Create ( const ci::fs::path & filePath, const Format & fmt )
        {
            if ( !fs::exists ( filePath ) ) return nullptr;
            return MediaPlayer::Create ( loadFile ( filePath ), fmt );
        }

        MediaPlayer::MediaPlayer ( const ci::DataSourceRef & source, const Format& fmt )
            : _format ( fmt )
        {
            _impl = std::make_unique<Impl> ( *this, source, _format );
            _updateConnection = app::App::get ( )->getSignalUpdate ( ).connect ( [=] { Update ( ); } );
        }

        bool MediaPlayer::Update ( )
        {
            return _impl->Update ( );
        }

        void MediaPlayer::Play ( )
        {
            _impl->Play ( );
        }

        void MediaPlayer::Pause ( )
        {
            _impl->Pause ( );
        }

        void MediaPlayer::TogglePlayback ( )
        {
            _impl->TogglePlayback ( );
        }

        bool MediaPlayer::SetPlaybackRate ( float rate )
        {
            return _impl->SetPlaybackRate ( rate );
        }

        float MediaPlayer::GetPlaybackRate ( ) const
        {
            return _impl->GetPlaybackRate ( );
        }

        bool MediaPlayer::IsPlaybackRateSupported ( float rate ) const
        {
            return _impl->IsPlaybackRateSupported ( rate );
        }

        void MediaPlayer::SetMuted ( bool mute )
        {
            _impl->SetMuted ( mute );
        }

        bool MediaPlayer::IsMuted ( ) const
        {
            return _impl->IsMuted ( );
        }

        void MediaPlayer::SetVolume ( float volume )
        {
            _impl->SetVolume ( volume );
        }

        float MediaPlayer::GetVolume ( ) const
        {
            return _impl->GetVolume ( );
        }

        void MediaPlayer::SetLoop ( bool loop )
        {
            _impl->SetLoop ( loop );
        }

        bool MediaPlayer::IsLooping ( ) const
        {
            return _impl->IsLooping ( );
        }

        const ivec2 & MediaPlayer::GetSize ( ) const
        {
            return _impl->GetSize ( );
        }

        void MediaPlayer::SeekToSeconds ( float seconds, bool approximate )
        {
            return _impl->SeekToSeconds ( seconds, approximate );
        }

        void MediaPlayer::SeekToPercentage ( float normalizedTime, bool approximate )
        {
            return _impl->SeekToPercentage ( std::clamp ( normalizedTime, 0.0f, 1.0f ), approximate );
        }

        bool MediaPlayer::IsComplete ( ) const
        {
            return _impl->IsComplete ( );
        }

        bool MediaPlayer::IsPaused ( ) const
        {
            return _impl->IsPaused ( );
        }

        bool MediaPlayer::IsPlaying ( ) const
        {
            return _impl->IsPlaying ( );
        }

        bool MediaPlayer::IsSeeking ( ) const
        {
            return _impl->IsSeeking ( );
        }

        bool MediaPlayer::IsReady ( ) const
        {
            return _impl->IsReady ( );
        }

        bool MediaPlayer::HasAudio ( ) const
        {
            return _impl->HasAudio ( );
        }

        bool MediaPlayer::HasVideo ( ) const
        {
            return _impl->HasVideo ( );
        }

        float MediaPlayer::GetPositionInSeconds ( ) const
        {
            return _impl->GetPositionInSeconds ( );
        }

        float MediaPlayer::GetDurationInSeconds ( ) const
        {
            return _impl->GetDurationInSeconds ( );
        }

        bool MediaPlayer::CheckNewFrame ( ) const
        {
            return _impl->CheckNewFrame ( );
        }

        const Surface8uRef & MediaPlayer::GetSurface ( ) const
        {
            return _impl->GetSurface ( );
        }

        MediaPlayer::FrameLeaseRef MediaPlayer::GetTexture ( ) const
        {
            return _impl->GetTexture ( );
        }

        MediaPlayer::~MediaPlayer ( )
        {
            _impl = nullptr;
            _updateConnection.disconnect ( );
        }
    }
}