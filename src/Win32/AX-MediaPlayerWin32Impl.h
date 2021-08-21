//
//  AX-MediaPlayerWin32Impl.h
//  AX-MediaPlayer
//
//  Created by Andrew Wright (@axjxwright) on 17/08/21.
//  (c) 2021 AX Interactive (axinteractive.com.au)
//

#pragma once

#ifdef WIN32

#ifdef WINVER
    #undef WINVER
#endif
#define WINVER _WIN32_WINNT_WIN10

#include <wrl/client.h>
#include <mfapi.h>
#include <mfmediaengine.h>
#include <wincodec.h>

using namespace Microsoft::WRL;

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")

#else

#error("Unsupported platform")

#endif

#include "AX-MediaPlayer.h"

namespace AX::Video
{
    void RunSynchronousInMTAThread  ( std::function<void ( )> callback );
    void RunSynchronousInMainThread ( std::function<void ( )> callback );

    class MediaPlayer::Impl : public IMFMediaEngineNotify
    {
    public:
        class RenderPath
        {
        public:

            RenderPath ( MediaPlayer::Impl& owner, const ci::DataSourceRef & source, uint32_t flags )
                : _owner ( owner )
                , _source ( source )
                , _flags ( flags )
            { }

            virtual ~RenderPath ( ) { };
            
            virtual bool Initialize ( IMFAttributes & attributes ) { return true; }
            virtual bool InitializeRenderTarget ( const ci::ivec2 & size ) = 0;
            virtual bool ProcessFrame ( ) = 0;
            virtual MediaPlayer::FrameLeaseRef GetFrameLease ( ) const { return nullptr; }
            inline const ci::ivec2 & GetSize ( ) const { return _size; };


        protected:
            uint32_t            _flags{ 0 };
            ci::DataSourceRef   _source;
            MediaPlayer::Impl & _owner;
            ci::ivec2           _size;
        };

        using RenderPathRef = std::unique_ptr<RenderPath>;
        friend class RenderPath;
        friend class DXGIRenderPath;
        friend class WICRenderPath;

        Impl    ( MediaPlayer & owner, const ci::DataSourceRef & source, uint32_t flags );

        bool    Update ( );

        bool    IsComplete ( ) const;
        bool    IsPlaying ( ) const;
        bool    IsPaused ( ) const;
        bool    IsSeeking ( ) const;

        bool    HasAudio ( ) const;
        bool    HasVideo ( ) const;

        void    Play ( );
        void    Pause ( );
        void    TogglePlayback ( );

        void    SetPlaybackRate ( float rate );
        float   GetPlaybackRate ( ) const;

        void    SetMuted ( bool mute );
        bool    IsMuted ( ) const;

        void    SetVolume ( float volume );
        float   GetVolume ( ) const;

        void    SetLoop ( bool loop );
        bool    IsLooping ( ) const;

        const   ci::ivec2 & GetSize ( ) const { return _size; }

        void    SeekToSeconds ( float seconds );
        void    SeekToPercentage ( float normalizedTime );

        float   GetPositionInSeconds ( ) const;
        float   GetDurationInSeconds ( ) const { return _duration; }

        bool    CheckNewFrame ( ) const { return _hasNewFrame.load ( ); }
        const   ci::Surface8uRef & GetSurface ( ) const;
        MediaPlayer::FrameLeaseRef GetTexture ( ) const;

        HRESULT STDMETHODCALLTYPE EventNotify ( DWORD event, DWORD_PTR param1, DWORD param2 ) override;
        HRESULT STDMETHODCALLTYPE QueryInterface ( REFIID riid, LPVOID * ppvObj ) override;
        ULONG STDMETHODCALLTYPE AddRef ( ) override;
        ULONG STDMETHODCALLTYPE Release ( ) override;

        ~Impl ( );


    protected:

        MediaPlayer &               _owner;
        ci::DataSourceRef           _source;
        ci::ivec2                   _size;
        uint32_t                    _flags{ 0 };
        float                       _duration{ 0.0f };
        ci::Surface8uRef            _surface{ nullptr };
        RenderPathRef               _renderPath;
        ComPtr<IMFMediaEngine>      _mediaEngine{ nullptr };
        mutable std::atomic_bool    _hasNewFrame{ false };
    };
}
