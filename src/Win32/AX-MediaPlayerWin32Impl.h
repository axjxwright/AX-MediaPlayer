//
//  AX-MediaPlayerWin32Impl.h
//  AX-MediaPlayer
//
//  Created by Andrew Wright on 17/08/21.
//  (c) 2021 AX Interactive
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
    class MediaPlayer::Impl : public IMFMediaEngineNotify
    {
    public:
        Impl ( MediaPlayer & owner, const ci::DataSourceRef & source );

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

        const   ci::Surface8uRef & GetCurrentSurface ( ) const { return _surface; }

        HRESULT STDMETHODCALLTYPE EventNotify ( DWORD event, DWORD_PTR param1, DWORD param2 ) override;
        HRESULT STDMETHODCALLTYPE QueryInterface ( REFIID riid, LPVOID * ppvObj ) override;
        ULONG STDMETHODCALLTYPE AddRef ( ) override;
        ULONG STDMETHODCALLTYPE Release ( ) override;

        ~Impl ( );


    protected:

        void CreateBackingBitmap ( int w, int h );

        MediaPlayer & _owner;
        ci::DataSourceRef _source;
        ci::ivec2 _size;
        float _duration{ 0.0f };
        ci::Surface8uRef _surface{ nullptr };
        ULONG _refCount{ 0 };

        ComPtr<IMFMediaEngine> _mediaEngine{ nullptr };
        ComPtr<IWICBitmap> _wicBitmap{ nullptr };
    };
}
