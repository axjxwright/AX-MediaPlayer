//
//  AX-MediaPlayerMSWImpl.cxx
//  AX-MediaPlayer
//
//  Created by Andrew Wright (@axjxwright) on 11/12/21.
//  (c) 2021 AX Interactive (axinteractive.com.au)
//

#include "AX-MediaPlayerOSXImpl.h"
#include "cinder/app/App.h"
#include <AVFoundation/AVFoundation.h>

using namespace ci;

namespace
{
    class StaticFrameLease : public AX::Video::MediaPlayer::FrameLease
    {
    public:
        StaticFrameLease ( const gl::TextureRef& texture )
        : _texture ( texture )
        { }
        gl::TextureRef ToTexture ( ) const override { return _texture; }

    protected:
        gl::TextureRef  _texture;
        bool IsValid ( ) const override { return _texture != nullptr; };
    };
}

namespace AX::Video
{
    MediaPlayer::Impl::Impl ( MediaPlayer & owner, const DataSourceRef & source, const Format& format )
        : _owner ( owner )
        , _source ( source )
        , _format( format )
    {
        try
        {
            if ( format.IsHardwareAccelerated() )
            {
                if ( source->isUrl() )
                {
                    _player = qtime::MovieGl::create( source->getUrl() );
                }else
                {
                    _player = qtime::MovieGl::create ( source->getFilePath() );
                }
            }else
            {
                if ( source->isUrl() )
                {
                    _player = qtime::MovieSurface::create( source->getUrl() );
                }else
                {
                    _player = qtime::MovieSurface::create ( source->getFilePath() );
                }
            }
            
            if ( _player && !_format.IsAudioEnabled() )
            {
                SetMuted( true );
            }
            
            if ( _player )
            {
                _duration = _player->getDuration();
                _size = _player->getSize();
                
                _player->getReadySignal().connect( [=]
                {
                    _duration = _player->getDuration();
                    _size = _player->getSize();
                    _owner.OnReady.emit();
                } );
                _player->getEndedSignal().connect( [=]
                {
                    _isPlaying = false;
                    _owner.OnComplete.emit();
                } );
            }
        }catch ( const std::exception& e )
        {
            // @note(andrew): Run this next frame so the client has a chance to
            // actually connect to the OnError signal
            app::App::get()->dispatchAsync ( [=]
            {
                _owner.OnError.emit ( MediaPlayer::Error::SourceNotSupported );
            } );
        }
    }

    void MediaPlayer::Impl::Play ( )
    {
        if ( _player && !_isPlaying )
        {
            _isPlaying = true;
            _player->play ( );
        }
    }

    void MediaPlayer::Impl::Pause ( )
    {
        if ( _player && _isPlaying )
        {
            _isPlaying = false;
            _player->play ( true );
        }
    }

    void MediaPlayer::Impl::TogglePlayback ( )
    {
        if ( IsPaused ( ) )
        {
            Play ( );
        }
        else
        {
            Pause ( );
        }
    }

    bool MediaPlayer::Impl::SetPlaybackRate ( float rate )
    {
        if ( _player )
        {
            if ( IsPlaybackRateSupported ( rate ) )
            {
                if ( _player->setRate ( rate ) )
                {
                    _playbackRate = rate;
                    return true;
                }
            }
        }

        return false;
    }

    float MediaPlayer::Impl::GetPlaybackRate ( ) const
    {
        return _playbackRate;
    }

    bool MediaPlayer::Impl::IsPlaybackRateSupported ( float rate ) const
    {
        if ( !_player ) return false;

        if ( rate == 0.0f || rate == 1.0f ) return true;
        
        AVPlayer * player = _player->getPlayerHandle();
        AVPlayerItem * item = [player currentItem];
        
        if ( item )
        {
            // @note(andrew): These seem like reasonable guesses to me
            // but if there's an official source outlining what consitutes
            // slow vs fast, please let me know.
            
            bool isSlowForward = rate > 0.0f && rate < 1.0f;
            bool isFastForward = rate > 1.0f;
            
            bool isSlowReverse = rate < 0.0f && rate > -1.0f;
            bool isFastReverse = rate < -1.0f;
            
            if ( isSlowForward ) return [item canPlaySlowForward];
            if ( isFastForward ) return [item canPlayFastForward];
            
            if ( isSlowReverse ) return [item canPlaySlowReverse];
            if ( isFastReverse ) return [item canPlayFastReverse];
        }
        
        return false;
    }

    void MediaPlayer::Impl::SetMuted ( bool mute )
    {
        if ( _player )
        {
            _player->setVolume( mute ? 0.0f : _volume );
        }
    }

    bool MediaPlayer::Impl::IsMuted ( ) const
    {
        if ( _player )
        {
            return _player->getVolume() == 0.0f;
        }

        return false;
    }

    void MediaPlayer::Impl::SetVolume ( float volume )
    {
        if ( _player )
        {
            _volume = volume;
            _player->setVolume( volume );
        }
    }

    float MediaPlayer::Impl::GetVolume ( ) const
    {
        if ( _player )
        {
            return _player->getVolume();
        }

        return 1.0f;
    }

    void MediaPlayer::Impl::SetLoop ( bool loop )
    {
        if ( _player )
        {
            _loop = loop;
            _player->setLoop ( loop );
        }
    }

    bool MediaPlayer::Impl::IsLooping ( ) const
    {
        return _loop;
    }

    float MediaPlayer::Impl::GetPositionInSeconds ( ) const
    {
        if ( !_player ) return -1.0f;
        return static_cast<float> ( _player->getCurrentTime() );
    }

    void MediaPlayer::Impl::SeekToSeconds ( float seconds, bool approximate )
    {
        if ( _player )
        {
            _owner.OnSeekStart.emit();
            _player->seekToTime( seconds );
            
            // @note(andrew): Unfortunately using the jumped signal isn't viable
            // as it gets called in a lot more cases than just seeking. Just firing
            // these signals for symmetry with the windows version but it's not accurate
            _owner.OnSeekEnd.emit();
        }
    }

    void MediaPlayer::Impl::SeekToPercentage ( float normalizedTime, bool approximate )
    {
        if ( _duration > 0.0f )
        {
            SeekToSeconds ( normalizedTime * _duration, approximate );
        }
    }

    bool MediaPlayer::Impl::IsComplete ( ) const
    {
        if ( _player )
        {
            return _player->isDone();
        }
        else
        {
            return true;
        }
    }

    bool MediaPlayer::Impl::IsPaused ( ) const
    {
        return !_isPlaying;
    }

    bool MediaPlayer::Impl::IsPlaying ( ) const
    {
        return !IsPaused ( );
    }

    bool MediaPlayer::Impl::IsSeeking ( ) const
    {
        return false;
    }

    bool MediaPlayer::Impl::IsReady ( ) const
    {
        if ( _player )
        {
            return _player->isPlayable();
        }
        return false;
    }

    bool MediaPlayer::Impl::HasAudio ( ) const
    {
        if ( _player )
        {
            return _player->hasAudio ( );
        }

        return false;
    }

    bool MediaPlayer::Impl::HasVideo ( ) const
    {
        if ( _player )
        {
            return _player->hasVisuals ( );
        }

        return false;
    }

    bool MediaPlayer::Impl::Update ( )
    {
        if ( _player )
        {
            if ( _player->checkNewFrame() )
            {
                _hasNewFrame.store( true );
                if ( !_format.IsHardwareAccelerated() )
                {
                    _surface = std::static_pointer_cast<qtime::MovieSurface>( _player )->getSurface();
                }
            }
            
            if ( _source->isUrl() )
            {
                AVPlayer * player = _player->getPlayerHandle();
                AVPlayerItem * item = [player currentItem];
                
                if ( item )
                {
                    bool isBuffering = ![item isPlaybackLikelyToKeepUp] && [item isPlaybackBufferEmpty];
                    if ( isBuffering && !_wasBuffering )
                    {
                        _owner.OnBufferingStart.emit();
                    }else if ( !isBuffering && _wasBuffering )
                    {
                        _owner.OnBufferingEnd.emit();
                    }
                    
                    _wasBuffering = isBuffering;
                }
            }
        }

        return false;
    }

    const Surface8uRef & MediaPlayer::Impl::GetSurface ( ) const
    {
        _hasNewFrame.store ( false );
        return _surface;
    }

    MediaPlayer::FrameLeaseRef MediaPlayer::Impl::GetTexture ( ) const
    {
        if ( _player )
        {
            if ( _format.IsHardwareAccelerated() )
            {
                _hasNewFrame.store ( false );
                auto player = std::static_pointer_cast<qtime::MovieGl>( _player );
                return std::make_unique<StaticFrameLease>( player->getTexture() );
            }else
            {
                _hasNewFrame.store ( false );
                auto player = std::static_pointer_cast<qtime::MovieSurface>( _player );
                if ( player && player->getSurface() )
                {
                    auto texture = gl::Texture::create ( *player->getSurface(), gl::Texture::Format ( ).loadTopDown ( ) );
                    return std::make_unique<StaticFrameLease>( texture );
                }else
                {
                    return nullptr;
                }
            }
        }
        
        return nullptr;
    }
}
