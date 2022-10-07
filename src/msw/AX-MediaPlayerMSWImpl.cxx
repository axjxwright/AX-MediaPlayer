//
//  AX-MediaPlayerMSWImpl.cxx
//  AX-MediaPlayer
//
//  Created by Andrew Wright (@axjxwright) on 17/08/21.
//  (c) 2021 AX Interactive (axinteractive.com.au)
//

#include "AX-MediaPlayerMSWImpl.h"
#include "AX-MediaPlayerMSWWICRenderPath.h"
#include "AX-MediaPlayerMSWDXGIRenderPath.h"

#include "cinder/app/App.h"
#include "cinder/DataSource.h"
#include "cinder/Log.h"
#include "cinder/audio/Device.h"
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
    static std::atomic_int kNumMediaFoundationInstances = 0;

    static void OnMediaPlayerCreated ( )
    {
        if ( kNumMediaFoundationInstances++ == 0 )
        {
            MFStartup ( MF_VERSION );
        }
    }

    static void OnMediaPlayerDestroyed ( )
    {
        if ( --kNumMediaFoundationInstances == 0 )
        {
            MFShutdown ( );
        }
    }

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
    // @note(andrew): Current only the audio related functions ( Mute / Volume ) seems
    // to mind about explicitly being run in the MTA thread, but if you're seeing any
    // weird crashes, the first thing to try is to wrap any interaction with _mediaEngine
    // in a call to RunSynchronousInMTAThread ( ... )

    void RunSynchronousInMTAThread ( std::function<void ( )> callback )
    {
        APTTYPE apartmentType = {};
        APTTYPEQUALIFIER qualifier = {};

        bool inited = SUCCEEDED ( CoGetApartmentType ( &apartmentType, &qualifier ) );
        assert ( inited );

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

    void RunSynchronousInMainThread ( std::function<void ( )> callback )
    {
        app::App::get ( )->dispatchSync ( [&] { callback ( ); } );
    }

    MediaPlayer::Impl::Impl ( MediaPlayer & owner, const DataSourceRef & source, const Format& format )
        : _owner ( owner )
        , _source ( source )
        , _format( format )
    {
        OnMediaPlayerCreated ( );

        ComPtr<IMFMediaEngineClassFactory> factory;
        if ( SUCCEEDED ( CoCreateInstance ( CLSID_MFMediaEngineClassFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS ( &factory ) ) ) )
        {
            ComPtr<IMFAttributes> attributes;
            MFCreateAttributes ( attributes.GetAddressOf ( ), 0 );
            attributes->SetUINT32 ( MF_MEDIA_ENGINE_VIDEO_OUTPUT_FORMAT, DXGI_FORMAT_B8G8R8A8_UNORM );
            attributes->SetUnknown ( MF_MEDIA_ENGINE_CALLBACK, this );

            DWORD flags = MF_MEDIA_ENGINE_REAL_TIME_MODE;

            if ( !_format.IsAudioEnabled() )
            {
                flags |= MF_MEDIA_ENGINE_FORCEMUTE;
            }

            if ( _format.IsAudioOnly() )
            {
                flags |= MF_MEDIA_ENGINE_AUDIOONLY;
            }

            if ( _format.IsHardwareAccelerated() )
            {
                _renderPath = std::make_unique<DXGIRenderPath> ( *this, source );
            }
            else
            {
                _renderPath = std::make_unique<WICRenderPath> ( *this, source );
            }

            if ( !_format.AudioDeviceID ( ).empty ( ) )
            {
                auto deviceId = _format.AudioDeviceID ( );
                std::wstring wideDeviceId{ deviceId.begin ( ), deviceId.end ( ) };

                if ( deviceId != audio::Device::getDefaultOutput ( )->getKey ( ) )
                {
                    CI_LOG_W ( "Non-default audio endpoints not currently supported" );
                }
                attributes->SetString ( MF_AUDIO_RENDERER_ATTRIBUTE_ENDPOINT_ID, wideDeviceId.c_str ( ) );
            }

            _renderPath->Initialize ( *attributes.Get() );

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

                _mediaEngine->QueryInterface ( _mediaEngineEx.GetAddressOf ( ) );
            }
        }
    }

    // @warn(andrew): This is not on the main thread, make sure to act accordingly!
    // i.e no GL activity here.

    HRESULT MediaPlayer::Impl::EventNotify ( DWORD event, DWORD_PTR param1, DWORD param2 )
    {
        {
            // @note(andrew): Make sure all signals are emitted on the main thread
            std::unique_lock<std::mutex> lk( _eventMutex );
            _eventQueue.push( Event{ event, param1, param2 } );
        }

        return S_OK;
    }

    void MediaPlayer::Impl::ProcessEvent ( DWORD evt, DWORD_PTR param1, DWORD param2 )
    {
        switch ( evt )
        {
            case MF_MEDIA_ENGINE_EVENT_DURATIONCHANGE:
            {
                _duration = static_cast< float > ( _mediaEngine->GetDuration() );
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_LOADEDMETADATA:
            {
                _duration = static_cast< float > ( _mediaEngine->GetDuration() );

                DWORD w, h;
                if( SUCCEEDED( _mediaEngine->GetNativeVideoSize( &w, &h ) ) )
                {
                    _size = ivec2( w, h );
                    _renderPath->InitializeRenderTarget( _size );
                }

                _hasMetadata = true;
                _owner.OnReady.emit();

                break;
            }

            case MF_MEDIA_ENGINE_EVENT_PLAY:
            {
                _owner.OnPlay.emit();
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_PAUSE:
            {
                _owner.OnPause.emit();
                break;
            }
            case MF_MEDIA_ENGINE_EVENT_ENDED:
            {
                _owner.OnComplete.emit();
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_SEEKING:
            {
                _owner.OnSeekStart.emit();
                _timeInSecondsAtStartOfSeek = _owner.GetPositionInSeconds ( );
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_SEEKED:
            {
                _owner.OnSeekEnd.emit();
                if ( _owner.IsLooping ( ) )
                {
                    // @NOTE(andrew): Since the complete event is not fired on looping videos,
                    // the best metric I was able to rely on was an EVENT_SEEKING with current time 0
                    // followed by an EVENT_SEEKED, also at time 0. My crude hack is to try and keep
                    // track of the current time when these are fired and if they're both zero 
                    // (or close to it) then to assume a loop has taken place. Not perfect, but 
                    // since the MFMediaEngine follows the HTML5 video spec, you can take it up
                    // with the brains trust at the W3C ;)

                    auto now = _owner.GetPositionInSeconds ( );
                    if ( now < 0.05f && ( ( now - _timeInSecondsAtStartOfSeek ) < 0.01f ) )
                    {
                        _owner.OnComplete.emit ( );
                    }
                }
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_BUFFERINGSTARTED:
            {
                _owner.OnBufferingStart.emit();
                break;
            }

            case MF_MEDIA_ENGINE_EVENT_BUFFERINGENDED:
            {
                _owner.OnBufferingEnd.emit();
                break;
            }   

            case MF_MEDIA_ENGINE_EVENT_ERROR:
            {
                MF_MEDIA_ENGINE_ERR error = static_cast< MF_MEDIA_ENGINE_ERR > ( param1 );
                _owner.OnError.emit( AXErrorFromMFError( error ) );
                break;
            }
        }
    }

    void MediaPlayer::Impl::UpdateEvents ( )
    {
        Event evt;
        bool hasEvent = false;
        do
        {
            hasEvent = false;
            {
                std::unique_lock<std::mutex> lk( _eventMutex );
                if( !_eventQueue.empty() )
                {
                    evt = _eventQueue.front();
                    _eventQueue.pop();
                    hasEvent = true;
                }
            }
            if( hasEvent )
            {
                ProcessEvent( evt.eventId, evt.param1, evt.param2 );
            }
        } while( hasEvent );
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

    bool MediaPlayer::Impl::SetPlaybackRate ( float rate )
    {
        if ( _mediaEngine )
        {
            if ( IsPlaybackRateSupported ( rate ) )
            {
                return SUCCEEDED ( _mediaEngine->SetPlaybackRate ( static_cast<double> ( rate ) ) );
            }
        }

        return false;
    }

    float MediaPlayer::Impl::GetPlaybackRate ( ) const
    {
        if ( _mediaEngine )
        {
            return static_cast<float> ( _mediaEngine->GetPlaybackRate ( ) );
        }

        return 1.0f;
    }

    bool MediaPlayer::Impl::IsPlaybackRateSupported ( float rate ) const
    {
        if ( _mediaEngineEx )
        {
            return _mediaEngineEx->IsPlaybackRateSupported ( static_cast<double> ( rate ) );
        }

        return false;
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

    void MediaPlayer::Impl::SeekToSeconds ( float seconds, bool approximate )
    {
        if ( _mediaEngineEx )
        {
            _mediaEngineEx->SetCurrentTimeEx ( seconds, approximate ? MF_MEDIA_ENGINE_SEEK_MODE_APPROXIMATE : MF_MEDIA_ENGINE_SEEK_MODE_NORMAL );
        }
        else if ( _mediaEngine )
        {
            _mediaEngine->SetCurrentTime ( static_cast<double> ( seconds ) );
        }
    }

    void MediaPlayer::Impl::SeekToPercentage ( float normalizedTime, bool approximate )
    {
        if ( _duration > 0.0f )
        {
            SeekToSeconds ( normalizedTime * _duration, approximate );
        }
    }

    void MediaPlayer::Impl::FrameStep ( int delta )
    {
        if ( _mediaEngineEx )
        {
            _mediaEngineEx->FrameStep ( delta > 0 ? true : false );
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

    bool MediaPlayer::Impl::IsReady ( ) const
    {
        return _hasMetadata;
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
                    _hasNewFrame.store ( true );
                }
            }
        }

        UpdateEvents ( );
        
        return false;
    }

    const Surface8uRef & MediaPlayer::Impl::GetSurface ( ) const
    {
        _hasNewFrame.store ( false );
        return _surface;
    }

    MediaPlayer::FrameLeaseRef MediaPlayer::Impl::GetTexture ( ) const
    {
        _hasNewFrame.store ( false );
        return _renderPath->GetFrameLease ( );
    }

    MediaPlayer::Impl::~Impl ( )
    {
        _renderPath = nullptr;
        _hasNewFrame.store ( false );
        
        // @todo(andrew): Do I need to ::Shutdown through the Ex interface
        // or since it's likely just an upcasted IMFMediaEngine will the
        // original interface suffice?
        _mediaEngineEx = nullptr;
        
        if ( _mediaEngine )
        {
            RunSynchronousInMTAThread ( [&]
            {
                _mediaEngine->Shutdown ( );
            } );

            _mediaEngine = nullptr;
        }

        OnMediaPlayerDestroyed ( );
    }
}