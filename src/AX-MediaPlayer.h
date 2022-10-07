//
//  AX-MediaPlayer.h
//  AX-MediaPlayer
//
//  Created by Andrew Wright (@axjxwright) on 17/08/21.
//  (c) 2021 AX Interactive (axinteractive.com.au)
//

#pragma once

#include "cinder/Cinder.h"
#include "cinder/Surface.h"
#include "cinder/Signals.h"
#include "cinder/gl/Texture.h"
#include "cinder/Filesystem.h"
#include "cinder/DataSource.h"
#include "cinder/Noncopyable.h"

namespace cinder
{
    namespace audio
    {
        using DeviceFwdRef = std::shared_ptr<class Device>;
    }
}

namespace AX::Video
{
    using MediaPlayerRef = std::shared_ptr<class MediaPlayer>;
    class MediaPlayer : public ci::Noncopyable
    {
    public:

        class Impl;
        
        enum class Error
        {
            NoError             = 0,
            Aborted             = 1,
            NetworkError        = 2,
            DecodingError       = 3,
            SourceNotSupported  = 4,
            Encrypted           = 5,
        };

        class FrameLease
        {
        public:
            virtual ~FrameLease ( ) { };

            operator bool ( ) const { return IsValid ( ); }
            operator ci::gl::TextureRef ( ) const { return ToTexture ( ); }
            virtual ci::gl::TextureRef ToTexture ( ) const { return nullptr; }

        protected:
            virtual bool IsValid ( ) const { return false; };
        };

        struct Format
        {
            Format & Audio ( bool enabled ) { _audioEnabled = enabled; return *this; }
            Format & AudioOnly ( bool audioOnly ) { _audioOnly = audioOnly; return *this; }
            Format & AudioDevice ( const ci::audio::DeviceFwdRef & device );
            Format & HardwareAccelerated ( bool accelerated ) { _hardwareAccelerated = accelerated; return *this; }

            bool    IsAudioEnabled ( ) const { return _audioEnabled;  }
            bool    IsAudioOnly ( ) const { return _audioOnly; }
            bool    IsHardwareAccelerated ( ) const { return _hardwareAccelerated; }
            const std::string & AudioDeviceID ( ) const { return _audioDeviceId; }

            Format ( ) { };

        protected:

            bool        _audioEnabled{ true };
            bool        _audioOnly{ false };
            bool        _hardwareAccelerated{ false };
            std::string _audioDeviceId{ "" };
        };

        using   FrameLeaseRef = std::unique_ptr<FrameLease>;
        
        using   EventSignal     = ci::signals::Signal<void ( )>;
        using   ErrorSignal     = ci::signals::Signal<void ( Error )>;

        static  MediaPlayerRef Create ( const ci::DataSourceRef & source, const Format & fmt = Format ( ) );
        static  MediaPlayerRef Create ( const ci::fs::path & filePath, const Format & fmt = Format ( ) );
        
        static  const std::string & ErrorToString ( Error error );
        inline const Format & GetFormat ( ) const { return _format; }

        void    Play ( );
        void    Pause ( );
        void    TogglePlayback ( );

        bool    SetPlaybackRate ( float rate );
        float   GetPlaybackRate ( ) const;
        bool    IsPlaybackRateSupported ( float rate ) const;

        void    SetMuted ( bool mute );
        bool    IsMuted  ( ) const;

        void    SetVolume ( float volume );
        float   GetVolume ( ) const;

        void    SetLoop ( bool loop );
        bool    IsLooping ( ) const;

        const   ci::ivec2& GetSize ( ) const;
        inline  ci::Area   GetBounds ( ) const { return ci::Area ( ci::ivec2(0), GetSize() ); }
        inline  bool       IsHardwareAccelerated ( ) const { return _format.IsHardwareAccelerated ( ); }

        bool    IsComplete ( ) const;
        bool    IsPlaying ( ) const;
        bool    IsPaused ( ) const;
        bool    IsSeeking ( ) const;
        bool    IsReady ( ) const;
            
        bool    HasAudio ( ) const;
        bool    HasVideo ( ) const;
        
        void    SeekToSeconds ( float seconds, bool approximate = false );
        void    SeekToPercentage ( float normalizedTime, bool approximate = false );

        void    FrameStep ( int delta );

        float   GetPositionInSeconds ( ) const;
        float   GetDurationInSeconds ( ) const;
        
        bool    CheckNewFrame ( ) const;

        const ci::Surface8uRef & GetSurface ( ) const;
        FrameLeaseRef GetTexture ( ) const;

        EventSignal OnReady;
        EventSignal OnComplete;
        EventSignal OnPlay;
        EventSignal OnPause;
        
        EventSignal OnSeekStart;
        EventSignal OnSeekEnd;

        EventSignal OnBufferingStart;
        EventSignal OnBufferingEnd;
        
        ErrorSignal OnError;

        ~MediaPlayer ( );

    protected:

        MediaPlayer ( const ci::DataSourceRef & source, const Format & format );
        bool Update ( );
        
        Format                   _format;
        std::unique_ptr<Impl>    _impl;
        ci::signals::Connection  _updateConnection;
    };
}