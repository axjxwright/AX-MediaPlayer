//
//  AX-MediaPlayerMSWImpl.h
//  AX-MediaPlayer
//
//  Created by Andrew Wright (@axjxwright) on 11/12/21.
//  (c) 2021 AX Interactive (axinteractive.com.au)
//

#pragma once

#ifdef __APPLE__

#include "cinder/qtime/QuickTimeGl.h"
#include "cinder/qtime/QuickTime.h"

#else

#error "Unsupported platform"

#endif

#include "AX-MediaPlayer.h"

namespace AX::Video
{
    class MediaPlayer::Impl
    {
    public:
        
        Impl    ( MediaPlayer & owner, const ci::DataSourceRef & source, const Format& format );

        bool    Update ( );

        bool    IsComplete ( ) const;
        bool    IsPlaying ( ) const;
        bool    IsPaused ( ) const;
        bool    IsSeeking ( ) const;
        bool    IsReady ( ) const;

        bool    HasAudio ( ) const;
        bool    HasVideo ( ) const;

        void    Play ( );
        void    Pause ( );
        void    TogglePlayback ( );

        bool    SetPlaybackRate ( float rate );
        float   GetPlaybackRate ( ) const;
        bool    IsPlaybackRateSupported ( float rate ) const;

        void    SetMuted ( bool mute );
        bool    IsMuted ( ) const;

        void    SetVolume ( float volume );
        float   GetVolume ( ) const;

        void    SetLoop ( bool loop );
        bool    IsLooping ( ) const;

        const   ci::ivec2 & GetSize ( ) const { return _size; }

        void    SeekToSeconds ( float seconds, bool approximate );
        void    SeekToPercentage ( float normalizedTime, bool approximate );

        float   GetPositionInSeconds ( ) const;
        float   GetDurationInSeconds ( ) const { return _duration; }

        bool    CheckNewFrame ( ) const { return _hasNewFrame.load ( ); }
        const   ci::Surface8uRef & GetSurface ( ) const;
        MediaPlayer::FrameLeaseRef GetTexture ( ) const;
        
    protected:
        
        using QtimePlayerRef        = std::shared_ptr<ci::qtime::MovieBase>;
        
        MediaPlayer &               _owner;
        ci::DataSourceRef           _source;
        ci::ivec2                   _size;
        MediaPlayer::Format         _format;
        float                       _duration{ 0.0f };
        ci::Surface8uRef            _surface{ nullptr };
        mutable std::atomic_bool    _hasNewFrame{ false };
        QtimePlayerRef              _player;
        bool                        _isPlaying{false};
        float                       _playbackRate{1.0f};
        float                       _volume{1.0f};
        bool                        _loop{false};
        bool                        _wasBuffering{false};
        
    };
}
