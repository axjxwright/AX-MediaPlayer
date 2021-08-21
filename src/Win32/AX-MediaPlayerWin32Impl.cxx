//
//  AX-MediaPlayerWin32Impl.cxx
//  AX-MediaPlayer
//
//  Created by Andrew Wright on 17/08/21.
//  (c) 2021 AX Interactive
//

#include "AX-MediaPlayerWin32Impl.h"
#include "AX-MediaPlayerWin32WICRenderPath.h"
#include "AX-MediaPlayerWin32DXGIRenderPath.h"

#include "cinder/DataSource.h"
#include <string>
#include <unordered_map>
#include <mutex>
#include <condition_variable>

#include <mfapi.h>
#include <mferror.h>
#include <mfmediaengine.h>

using namespace ci;

namespace
{
    class MFCallbackBase : public IMFAsyncCallback
    {
    public:
        MFCallbackBase ( DWORD flags = 0, DWORD queue = MFASYNC_CALLBACK_QUEUE_MULTITHREADED ) : m_flags ( flags ), m_queue ( queue ) {}
        virtual ~MFCallbackBase ( ) = default;

        DWORD GetQueue ( ) const { return m_queue; }
        DWORD GetFlags ( ) const { return m_flags; }

        IFACEMETHODIMP GetParameters ( _Out_ DWORD * flags, _Out_ DWORD * queue )
        {
            *flags = m_flags;
            *queue = m_queue;
            return S_OK;
        }

        HRESULT STDMETHODCALLTYPE QueryInterface ( REFIID riid, LPVOID * ppvObj )
        {
            if ( !ppvObj ) return E_INVALIDARG;
            
            *ppvObj = NULL;
            if ( riid == IID_IMFAsyncCallback )
            {                
                *ppvObj = (LPVOID)this;
                AddRef ( );
                return NOERROR;
            }
            return E_NOINTERFACE;
        }

        ULONG STDMETHODCALLTYPE AddRef ( )
        {
            InterlockedIncrement ( &m_refCount );
            return m_refCount;
        }

        ULONG STDMETHODCALLTYPE Release( )
        {
            ULONG count = InterlockedDecrement ( &m_refCount );
            if ( 0 == m_refCount )
            {
                delete this;
            }
            return count;
        }

    private:
        DWORD m_flags = 0;
        DWORD m_queue = 0;
        ULONG m_refCount = 0;
    };

    class MFWorkItem : public MFCallbackBase
    {
    public:
        MFWorkItem ( std::function<void ( )> callback, DWORD flags = 0, DWORD queue = MFASYNC_CALLBACK_QUEUE_MULTITHREADED ) : MFCallbackBase ( flags, queue )
        {
            m_callback = callback;
        }

        IFACEMETHODIMP Invoke ( _In_opt_ IMFAsyncResult * /*result*/ ) noexcept override
            try
        {
            m_callback ( );
            Release ( );
            return S_OK;
        }
        catch ( const std::exception & e )
        {
            std::cout << "Error: " << e.what ( ) << std::endl;
            return E_ABORT;
        }

    private:
        std::function<void ( )> m_callback;
    };

    inline void MFPutWorkItem ( std::function<void ( )> callback )
    {
        ComPtr<MFWorkItem> workItem{ new MFWorkItem ( callback ) };
        workItem->AddRef ( );

        // @note(andrew): ::Release()'d when ::Invoked()
        MFPutWorkItem2 ( workItem->GetQueue ( ), 0, workItem.Get ( ), nullptr );
    }

    // @note(andrew): Current only the audio related functions ( Mute / Volume ) seems
    // to mind about explicitly being run in the MTA thread, but if you're seeing any
    // weird crashes, the first thing to try is to wrap any interaction with _mediaEngine
    // in a call to RunSynchronousInMTAThread ( ... )

    inline void RunSynchronousInMTAThread ( std::function<void ( )> callback )
    {
        APTTYPE apartmentType = {};
        APTTYPEQUALIFIER qualifier = {};

        assert ( SUCCEEDED ( CoGetApartmentType ( &apartmentType, &qualifier ) ) );

        if ( apartmentType == APTTYPE_MTA )
        {
            // Already in the MTA thread, just run the code
            // @note(andrew): Do I need to do some co-init stuff here?
            callback ( );
        }
        else
        {
            std::condition_variable wait;
            std::mutex lock;
            std::atomic_bool isDone{ false };

            MFPutWorkItem ( [&] ( ) 
            {
                callback ( );
                isDone.store ( true );
                wait.notify_one ( );
            } );

            std::unique_lock lk{ lock };
            wait.wait ( lk, [&] { return isDone.load ( ); } );
        }
    }

    std::string MFEventToString ( MF_MEDIA_ENGINE_EVENT event )
    {
        static std::unordered_map<MF_MEDIA_ENGINE_EVENT, std::string> kMessages =
        {
            { MF_MEDIA_ENGINE_EVENT_LOADSTART, "MF_MEDIA_ENGINE_EVENT_LOADSTART" },
            { MF_MEDIA_ENGINE_EVENT_PROGRESS, "MF_MEDIA_ENGINE_EVENT_PROGRESS" },
            { MF_MEDIA_ENGINE_EVENT_SUSPEND, "MF_MEDIA_ENGINE_EVENT_SUSPEND" },
            { MF_MEDIA_ENGINE_EVENT_ABORT, "MF_MEDIA_ENGINE_EVENT_ABORT" },
            { MF_MEDIA_ENGINE_EVENT_ERROR, "MF_MEDIA_ENGINE_EVENT_ERROR" },
            { MF_MEDIA_ENGINE_EVENT_EMPTIED, "MF_MEDIA_ENGINE_EVENT_EMPTIED" },
            { MF_MEDIA_ENGINE_EVENT_STALLED, "MF_MEDIA_ENGINE_EVENT_STALLED" },
            { MF_MEDIA_ENGINE_EVENT_PLAY, "MF_MEDIA_ENGINE_EVENT_PLAY" },
            { MF_MEDIA_ENGINE_EVENT_PAUSE, "MF_MEDIA_ENGINE_EVENT_PAUSE" },
            { MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA, "MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA" },
            { MF_MEDIA_ENGINE_EVENT_LOADEDDATA, "MF_MEDIA_ENGINE_EVENT_LOADEDDATA" },
            { MF_MEDIA_ENGINE_EVENT_WAITING, "MF_MEDIA_ENGINE_EVENT_WAITING" },
            { MF_MEDIA_ENGINE_EVENT_PLAYING, "MF_MEDIA_ENGINE_EVENT_PLAYING" },
            { MF_MEDIA_ENGINE_EVENT_CANPLAY, "MF_MEDIA_ENGINE_EVENT_CANPLAY" },
            { MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH, "MF_MEDIA_ENGINE_EVENT_CANPLAYTHROUGH" },
            { MF_MEDIA_ENGINE_EVENT_SEEKING, "MF_MEDIA_ENGINE_EVENT_SEEKING" },
            { MF_MEDIA_ENGINE_EVENT_SEEKED, "MF_MEDIA_ENGINE_EVENT_SEEKED" },
            { MF_MEDIA_ENGINE_EVENT_TIMEUPDATE, "MF_MEDIA_ENGINE_EVENT_TIMEUPDATE" },
            { MF_MEDIA_ENGINE_EVENT_ENDED, "MF_MEDIA_ENGINE_EVENT_ENDED" },
            { MF_MEDIA_ENGINE_EVENT_RATECHANGE, "MF_MEDIA_ENGINE_EVENT_RATECHANGE" },
            { MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE, "MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE" },
            { MF_MEDIA_ENGINE_EVENT_VOLUMECHANGE, "MF_MEDIA_ENGINE_EVENT_VOLUMECHANGE" },
            { MF_MEDIA_ENGINE_EVENT_FORMATCHANGE, "MF_MEDIA_ENGINE_EVENT_FORMATCHANGE" },
            { MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS, "MF_MEDIA_ENGINE_EVENT_PURGEQUEUEDEVENTS" },
            { MF_MEDIA_ENGINE_EVENT_TIMELINE_MARKER, "MF_MEDIA_ENGINE_EVENT_TIMELINE_MARKER" },
            { MF_MEDIA_ENGINE_EVENT_BALANCECHANGE, "MF_MEDIA_ENGINE_EVENT_BALANCECHANGE" },
            { MF_MEDIA_ENGINE_EVENT_DOWNLOADCOMPLETE, "MF_MEDIA_ENGINE_EVENT_DOWNLOADCOMPLETE" },
            { MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED, "MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED" },
            { MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED, "MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED" },
            { MF_MEDIA_ENGINE_EVENT_FRAMESTEPCOMPLETED, "MF_MEDIA_ENGINE_EVENT_FRAMESTEPCOMPLETED" },
            { MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE, "MF_MEDIA_ENGINE_EVENT_NOTIFYSTABLESTATE" },
            { MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY, "MF_MEDIA_ENGINE_EVENT_FIRSTFRAMEREADY" },
            { MF_MEDIA_ENGINE_EVENT_TRACKSCHANGE, "MF_MEDIA_ENGINE_EVENT_TRACKSCHANGE" },
            { MF_MEDIA_ENGINE_EVENT_OPMINFO, "MF_MEDIA_ENGINE_EVENT_OPMINFO" },
            { MF_MEDIA_ENGINE_EVENT_RESOURCELOST, "MF_MEDIA_ENGINE_EVENT_RESOURCELOST" },
            { MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED, "MF_MEDIA_ENGINE_EVENT_DELAYLOADEVENT_CHANGED" },
            { MF_MEDIA_ENGINE_EVENT_STREAMRENDERINGERROR, "MF_MEDIA_ENGINE_EVENT_STREAMRENDERINGERROR" },
            { MF_MEDIA_ENGINE_EVENT_SUPPORTEDRATES_CHANGED, "MF_MEDIA_ENGINE_EVENT_SUPPORTEDRATES_CHANGED" },
            { MF_MEDIA_ENGINE_EVENT_AUDIOENDPOINTCHANGE, "MF_MEDIA_ENGINE_EVENT_AUDIOENDPOINTCHANGE" }
        };

        if ( kMessages.count ( event ) )
        {
            return kMessages.at ( event );
        }
        else
        {
            return "Unknown MFEvent: " + std::to_string ( event );
        }
    }

    // @note(andrew): These are one-to-one for now but may change if/when i add
    // different backends etc
    AX::Video::MediaPlayer::Error AXErrorFromMFError ( MF_MEDIA_ENGINE_ERR error )
    {
        return static_cast<AX::Video::MediaPlayer::Error> ( error );
    }

    struct SafeBSTR
    {
        SafeBSTR ( const std::wstring & str )
        {
            assert ( !str.empty ( ) );
            _str = SysAllocStringLen ( str.data ( ), static_cast<UINT> ( str.size ( ) ) );
        }

        operator BSTR ( ) const { return _str; }

        ~SafeBSTR ( )
        {
            SysReleaseString ( _str );
            _str = nullptr;
        }

    protected:

        BSTR _str{ nullptr };
    };
}

namespace AX::Video
{
    MediaPlayer::Impl::Impl ( MediaPlayer & owner, const DataSourceRef & source, uint32_t flags )
        : _owner ( owner )
        , _source ( source )
        , _flags ( flags )
    {
        MFStartup ( MF_VERSION );

        ComPtr<IMFMediaEngineClassFactory> factory;
        if ( SUCCEEDED ( CoCreateInstance ( CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS ( &factory ) ) ) )
        {
            ComPtr<IMFAttributes> attributes;
            MFCreateAttributes ( attributes.GetAddressOf ( ), 0 );
            attributes->SetUINT32 ( MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM );
            attributes->SetUnknown ( MF_MEDIA_ENGINE_CALLBACK, this );

            DWORD flags = MF_MEDIA_ENGINE_REAL_TIME_MODE;

            if ( _flags & MediaPlayer::NoAudio )
            {
                flags |= MF_MEDIA_ENGINE_FORCEMUTE;
            }

            if ( _flags & MediaPlayer::AudioOnly )
            {
                flags |= MF_MEDIA_ENGINE_AUDIOONLY;
            }

            if ( _flags & MediaPlayer::HardwareAccelerated )
            {
                _renderPath = std::make_unique<DXGIRenderPath> ( *this, source, _flags );
            }
            else
            {
                _renderPath = std::make_unique<WICRenderPath> ( *this, source, _flags );
            }

            if ( SUCCEEDED ( factory->CreateInstance ( flags, attributes.Get ( ), _mediaEngine.GetAddressOf ( ) ) ) )
            {
                std::wstring actualPath;
                if ( _source->isUrl ( ) )
                {
                    auto str = _source->getUrl ( ).str ( );
                    actualPath = { str.begin ( ), str.end ( ) };
                }
                else
                {
                    actualPath = _source->getFilePath ( ).wstring ( );
                }

                _mediaEngine->SetSource ( SafeBSTR{ actualPath } );
                _mediaEngine->Load ( );
            }
        }
    }

    HRESULT MediaPlayer::Impl::EventNotify ( DWORD event, DWORD_PTR param1, DWORD param2 )
    {
        switch ( event )
        {
            case MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE:
            {
                _duration = static_cast<float> ( _mediaEngine->GetDuration ( ) );
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
            {
                DWORD w, h;
                _mediaEngine->GetNativeVideoSize ( &w, &h );
                _size = ivec2 ( w, h );
                _duration = static_cast<float> ( _mediaEngine->GetDuration ( ) );
                _renderPath->InitializeRenderTarget ( _size );

                break;
            }

            case MF_MEDIA_ENGINE_EVENT_PLAY:
            {
                _owner.OnPlay.emit ( );
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_PAUSE:
            {
                _owner.OnPause.emit ( );
                break;
            }
            case MF_MEDIA_ENGINE_EVENT_ENDED:
            {
                _owner.OnComplete.emit ( );
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_SEEKING:
            {
                _owner.OnSeekStart.emit ( );
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_SEEKED:
            {
                _owner.OnSeekEnd.emit ( );
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED :
            {
                _owner.OnBufferingStart.emit ( );
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED:
            {
                _owner.OnBufferingEnd.emit ( );
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_ERROR:
            {
                MF_MEDIA_ENGINE_ERR error = static_cast<MF_MEDIA_ENGINE_ERR> ( param1 );
                _owner.OnError.emit ( AXErrorFromMFError ( error ) );
                break;
            }
        }

        return S_OK;
    }

    HRESULT STDMETHODCALLTYPE MediaPlayer::Impl::QueryInterface ( REFIID riid, LPVOID * ppvObj )
    {
        if ( __uuidof( IMFMediaEngineNotify ) == riid )
        {
            *ppvObj = static_cast<IMFMediaEngineNotify *>( this );
        }
        else
        {
            *ppvObj = nullptr;
            return E_NOINTERFACE;
        }

        AddRef ( );

        return S_OK;
    }

    // @note(andrew): This memory is owned by the std::unique_ptr<Impl>
    // so don't do any reference counting here
    ULONG STDMETHODCALLTYPE MediaPlayer::Impl::AddRef ( ) { return 0;  }
    ULONG STDMETHODCALLTYPE MediaPlayer::Impl::Release ( ) { return 0; }
    
    void MediaPlayer::Impl::Play ( )
    {
        if ( _mediaEngine )
        {
            _mediaEngine->Play ( );
        }
    }

    void MediaPlayer::Impl::Pause ( )
    {
        if ( _mediaEngine )
        {
            _mediaEngine->Pause ( );
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

    void MediaPlayer::Impl::SetPlaybackRate ( float rate )
    {
        if ( _mediaEngine )
        {
            _mediaEngine->SetPlaybackRate ( rate );
        }
    }

    float MediaPlayer::Impl::GetPlaybackRate ( ) const
    {
        if ( _mediaEngine )
        {
            return static_cast<float> ( _mediaEngine->GetPlaybackRate ( ) );
        }

        return 1.0f;
    }

    void MediaPlayer::Impl::SetMuted ( bool mute )
    {
        if ( _mediaEngine )
        {
            RunSynchronousInMTAThread ( [&] { _mediaEngine->SetMuted ( mute ); } );
        }
    }

    bool MediaPlayer::Impl::IsMuted ( ) const
    {
        if ( _mediaEngine )
        {
            return static_cast<bool> ( _mediaEngine->GetMuted ( ) );
        }

        return false;
    }

    void MediaPlayer::Impl::SetVolume ( float volume )
    {
        if ( _mediaEngine )
        {
            RunSynchronousInMTAThread ( [&] { _mediaEngine->SetVolume ( volume ); } );
        }
    }

    float MediaPlayer::Impl::GetVolume ( ) const
    {
        if ( _mediaEngine )
        {
            return static_cast<float> ( _mediaEngine->GetVolume ( ) );
        }

        return 1.0f;
    }

    void MediaPlayer::Impl::SetLoop ( bool loop )
    {
        if ( _mediaEngine )
        {
            _mediaEngine->SetLoop ( static_cast<BOOL> ( loop ) );
        }
    }

    bool MediaPlayer::Impl::IsLooping ( ) const
    {
        if ( _mediaEngine )
        {
            return static_cast<bool> ( _mediaEngine->GetLoop ( ) );
        }

        return false;
    }

    float MediaPlayer::Impl::GetPositionInSeconds ( ) const
    {
        if ( !_mediaEngine ) return -1.0f;
        return static_cast<float> ( _mediaEngine->GetCurrentTime ( ) );
    }

    void MediaPlayer::Impl::SeekToSeconds ( float seconds )
    {
        if ( _mediaEngine )
        {
            _mediaEngine->SetCurrentTime ( static_cast<double> ( seconds ) );
        }
    }

    void MediaPlayer::Impl::SeekToPercentage ( float normalizedTime )
    {
        if ( _duration > 0.0f )
        {
            SeekToSeconds ( normalizedTime * _duration );
        }
    }

    bool MediaPlayer::Impl::IsComplete ( ) const
    {
        if ( _mediaEngine )
        {
            return _mediaEngine->IsEnded ( );
        }
        else
        {
            return true;
        }
    }

    bool MediaPlayer::Impl::IsPaused ( ) const
    {
        if ( _mediaEngine )
        {
            return _mediaEngine->IsPaused ( );
        }

        return false;
    }

    bool MediaPlayer::Impl::IsPlaying ( ) const
    {
        return !IsPaused ( );
    }

    bool MediaPlayer::Impl::IsSeeking ( ) const
    {
        if ( _mediaEngine )
        {
            return _mediaEngine->IsSeeking ( );
        }

        return false;
    }

    bool MediaPlayer::Impl::HasAudio ( ) const
    {
        if ( _mediaEngine )
        {
            return _mediaEngine->HasAudio ( );
        }

        return false;
    }

    bool MediaPlayer::Impl::HasVideo ( ) const
    {
        if ( _mediaEngine )
        {
            return _mediaEngine->HasVideo ( );
        }

        return false;
    }

    bool MediaPlayer::Impl::Update ( )
    {
        if ( _mediaEngine && HasVideo ( ) )
        {
            LONGLONG time;
            if ( SUCCEEDED ( _mediaEngine->OnVideoStreamTick ( &time ) ) )
            {
                if ( _renderPath->ProcessFrame ( ) )
                {
                    _owner.OnFrameReady.emit ( );
                }
            }
        }

        return false;
    }

    MediaPlayer::Impl::~Impl ( )
    {
        if ( _mediaEngine )
        {
            _mediaEngine->Shutdown ( );
            _mediaEngine = nullptr;
        }

        MFShutdown ( );
    }
}