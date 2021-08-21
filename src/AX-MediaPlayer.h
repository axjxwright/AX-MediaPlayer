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

        enum Flags
        {
            HardwareAccelerated = 0x01,
            NoAudio             = 0x02,
            AudioOnly           = 0x04
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

        using   FrameLeaseRef = std::unique_ptr<FrameLease>;
        
        using   EventSignal     = ci::signals::Signal<void ( )>;
        using   ErrorSignal     = ci::signals::Signal<void ( Error )>;

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
        inline  ci::Area   GetBounds ( ) const { return ci::Area ( ci::ivec2(0), GetSize() ); }
        inline  bool       IsHardwareAccelerated ( ) const { return _flags & HardwareAccelerated; }

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
        
        bool    CheckNewFrame ( ) const;

        const ci::Surface8uRef & GetSurface ( ) const;
        FrameLeaseRef GetTexture ( ) const;

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
        
        uint32_t                 _flags{ 0 };
        std::unique_ptr<Impl>    _impl;
        ci::signals::Connection  _updateConnection;
    };
}