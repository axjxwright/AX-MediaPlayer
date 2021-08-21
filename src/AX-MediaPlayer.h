//
//  AX-MediaPlayer.h
//  AX-MediaPlayer
//
//  Created by Andrew Wright on 17/08/21.
//  (c) 2021 AX Interactive
//

#pragma once

#include "cinder/Cinder.h"
#include "cinder/Surface.h"
#include "cinder/Signals.h"
#include "cinder/Filesystem.h"
#include "cinder/DataSource.h"
#include "cinder/Noncopyable.h"

namespace AX::Video
{
    using MediaPlayerRef = std::shared_ptr<class MediaPlayer>;
    class MediaPlayer : public ci::Noncopyable
    {
    public:

        class Impl;
        enum class Error
        {
            NoError = 0,
            Aborted = 1,
            NetworkError = 2,
            DecodingError = 3,
            SourceNotSupported = 4,
            Encrypted = 5,
        };

        enum Flags
        {
            HardwareAccelerated = 0x01,
            NoAudio             = 0x02,
            AudioOnly           = 0x04
        };

        using   EventSignal = ci::signals::Signal<void ( )>;
        using   ErrorSignal = ci::signals::Signal<void ( Error )>;

        static  MediaPlayerRef Create ( const ci::DataSourceRef & source, uint32_t flags = 0 );
        static  const std::string & ErrorToString ( Error error );

        void    Play ( );
        void    Pause ( );
        void    TogglePlayback ( );

        void    SetPlaybackRate ( float rate );
        float   GetPlaybackRate ( ) const;

        void    SetMuted ( bool mute );
        bool    IsMuted  ( ) const;

        void    SetVolume ( float volume );
        float   GetVolume ( ) const;

        void    SetLoop ( bool loop );
        bool    IsLooping ( ) const;

        const   ci::ivec2& GetSize ( ) const;
        inline  ci::Area GetBounds ( ) const { return ci::Area ( ci::ivec2(0), GetSize() ); }
        
        bool    IsComplete ( ) const;
        bool    IsPlaying ( ) const;
        bool    IsPaused ( ) const;
        bool    IsSeeking ( ) const;
            
        bool    HasAudio ( ) const;
        bool    HasVideo ( ) const;

        void    SeekToSeconds ( float seconds );
        void    SeekToPercentage ( float normalizedTime );

        float   GetPositionInSeconds ( ) const;
        float   GetDurationInSeconds ( ) const;
            
        const ci::Surface8uRef & GetCurrentSurface ( ) const;

        EventSignal OnFrameReady;
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

        MediaPlayer ( const ci::DataSourceRef & source, uint32_t flags );
        bool Update ( );
        
        std::unique_ptr<Impl> _impl;
        ci::signals::Connection _updateConnection;
    };
}