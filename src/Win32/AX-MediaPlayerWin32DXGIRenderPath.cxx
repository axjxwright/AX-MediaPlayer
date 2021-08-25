//
//  AX-MediaPlayerWin32DXGIRenderPath.cxx
//  AX-MediaPlayer
//
//  Created by Andrew Wright (@axjxwright) on 17/08/21.
//  (c) 2021 AX Interactive (axinteractive.com.au)
//


#pragma comment(lib, "d3d11.lib")

#include "AX-MediaPlayerWin32DXGIRenderPath.h"
#include "glad/glad_wgl.h"

#include "cinder/app/App.h"

using namespace ci;

namespace AX::Video
{
    using SharedTexture                 = DXGIRenderPath::SharedTexture;
    using SharedTextureRef              = DXGIRenderPath::SharedTextureRef;
    using InteropContextRef             = std::unique_ptr<class InteropContext>;

    // @note(andrew): Have a single D3D + interop context for all video sessions
    class InteropContext                : public ci::Noncopyable
    {
    public:

        static InteropContext &         Get ( );

        ~InteropContext                 ( );

        inline ID3D11Device *           Device ( ) const { return _device.Get(); }
        inline HANDLE                   Handle ( ) const { return _interopHandle; }
        inline IMFDXGIDeviceManager *   DXGIManager ( ) const { return _dxgiManager.Get ( ); }
        
        SharedTextureRef                CreateSharedTexture ( const ivec2 & size );
        inline bool                     IsValid ( ) const { return _isValid; }
        
    protected:

        InteropContext                  ( );

        ComPtr<ID3D11Device>            _device{ nullptr };
        ComPtr<IMFDXGIDeviceManager>    _dxgiManager{ nullptr };
        UINT                            _dxgiResetToken{ 0 };

        HANDLE                          _interopHandle{ nullptr };
        bool                            _isValid{ false };
    };

    // @note(andrew): Lazily initialize this but make sure
    // it hangs around for the remainer of the application
    // so that it outlives any of the players that depend on
    // it being alive and valid

    static std::unique_ptr<InteropContext> kInteropContext{ nullptr };
    InteropContext & InteropContext::Get ( )
    {
        if ( !kInteropContext ) kInteropContext.reset ( new InteropContext ( ) );
        return *kInteropContext;
    }

    class DXGIRenderPath::SharedTexture
    {
    public:

        SharedTexture               ( const ivec2 & size );
        ~SharedTexture              ( );

        bool                        Lock ( );
        bool                        Unlock ( );
        inline bool                 IsLocked ( ) const { return _isLocked;  }

        inline bool IsValid         ( ) const { return _isValid; }
        ID3D11Texture2D *           DXTextureHandle ( ) const { return _dxTexture.Get( ); }
        const ci::gl::TextureRef &  GLTextureHandle ( ) const { return _glTexture; }

    protected:

        ci::gl::TextureRef          _glTexture;
        ComPtr<ID3D11Texture2D>     _dxTexture{ nullptr };
        HANDLE                      _shareHandle{ nullptr };
        bool                        _isValid{ false };
        bool                        _isLocked{ false };
    };

    class DXGIRenderPathFrameLease : public MediaPlayer::FrameLease
    {
    public:

        DXGIRenderPathFrameLease ( const SharedTextureRef & texture )
            : _texture ( texture.get ( ) )
        {
            if ( _texture ) _texture->Lock ( );
        }

        inline bool    IsValid   ( ) const override { return ToTexture ( ) != nullptr; }
        gl::TextureRef ToTexture ( ) const override { return _texture ? _texture->GLTextureHandle ( ) : nullptr; };

        ~DXGIRenderPathFrameLease ( )
        {
            if ( _texture && _texture->IsLocked ( ) )
            {
                _texture->Unlock ( );
                _texture = nullptr;
            }
        }

    protected:

        SharedTexture * _texture{ nullptr };
    };

    InteropContext::InteropContext ( )
        : _isValid ( false )
    {
        UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;

#ifndef NDEBUG
        deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

        if ( !SUCCEEDED ( MFCreateDXGIDeviceManager ( &_dxgiResetToken, _dxgiManager.GetAddressOf ( ) ) ) ) return;
        if ( !SUCCEEDED ( D3D11CreateDevice ( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, nullptr, 0, D3D11_SDK_VERSION, _device.GetAddressOf ( ), nullptr, nullptr ) ) ) return;
        
        ComPtr<ID3D10Multithread> multiThread{ nullptr };
        if ( SUCCEEDED ( _device->QueryInterface ( multiThread.GetAddressOf ( ) ) ) )
        {
            multiThread->SetMultithreadProtected ( true );
        }
        else
        {
            return;
        }

        if ( !SUCCEEDED ( _dxgiManager->ResetDevice ( _device.Get ( ), _dxgiResetToken ) ) ) return;
        
        _interopHandle = wglDXOpenDeviceNV ( _device.Get ( ) );
        _isValid = _interopHandle != nullptr;
    }

    SharedTextureRef InteropContext::CreateSharedTexture ( const ivec2 & size )
    {
        auto texture = std::make_unique<SharedTexture> ( size );
        if ( texture->IsValid ( ) ) return std::move ( texture );

        return nullptr;
    }

    InteropContext::~InteropContext ( )
    {
        if ( _interopHandle != nullptr )
        {
            wglDXCloseDeviceNV ( _interopHandle );
            _interopHandle = nullptr;
        }

        _dxgiManager = nullptr;
        
        // @leak(andrew): Debug layer is whinging about live objects but is this 
        // this because the ComPtr destructors haven't had a chance to fire yet?
        #ifndef NDEBUG
        if ( _device )
        {
            ComPtr<ID3D11Debug> debug{ nullptr };
            if ( SUCCEEDED ( _device->QueryInterface ( debug.GetAddressOf ( ) ) ) )
            {
                debug->ReportLiveDeviceObjects ( D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL );
            }
        }
        #endif
    }

    DXGIRenderPath::SharedTexture::SharedTexture ( const ivec2 & size )
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = size.x;
        desc.Height = size.y;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET; // | D3D11_BIND_SHADER_RESOURCE;
        desc.Usage = D3D11_USAGE_DEFAULT;

        auto & context = InteropContext::Get ( );

        if ( SUCCEEDED ( context.Device()->CreateTexture2D ( &desc, nullptr, _dxTexture.GetAddressOf ( ) ) ) )
        {
            gl::Texture::Format fmt;
            fmt.internalFormat ( GL_RGBA ).loadTopDown ( );
            
            _glTexture = gl::Texture::create ( size.x, size.y, fmt );
            _shareHandle = wglDXRegisterObjectNV ( context.Handle(), _dxTexture.Get(), _glTexture->getId(), GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV );
            _isValid = _shareHandle != nullptr;
        }
    }

    bool DXGIRenderPath::SharedTexture::Lock ( )
    {
        assert ( !IsLocked ( ) );
        _isLocked = wglDXLockObjectsNV ( InteropContext::Get().Handle ( ), 1, &_shareHandle );
        return _isLocked;
    }

    bool DXGIRenderPath::SharedTexture::Unlock ( )
    {
        assert ( IsLocked ( ) );
        if ( wglDXUnlockObjectsNV ( InteropContext::Get ( ).Handle ( ), 1, &_shareHandle ) )
        {
            _isLocked = false;
            return true;
        }

        return false;
    }

    DXGIRenderPath::SharedTexture::~SharedTexture ( )
    {
        if ( _shareHandle != nullptr )
        {
            if ( IsLocked() ) wglDXUnlockObjectsNV ( InteropContext::Get ( ).Handle ( ), 1, &_shareHandle );
            wglDXUnregisterObjectNV ( InteropContext::Get ( ).Handle ( ), _shareHandle );
            _shareHandle = nullptr;
        }
    }

    DXGIRenderPath::DXGIRenderPath ( MediaPlayer::Impl & owner, const ci::DataSourceRef & source, uint32_t flags )
        : RenderPath ( owner, source, flags )
    { }

    bool DXGIRenderPath::Initialize ( IMFAttributes & attributes )
    {
        auto & interop = InteropContext::Get ( );
        if ( !interop.IsValid ( ) ) return false;

        if ( SUCCEEDED ( attributes.SetUnknown ( MF_MEDIA_ENGINE_DXGI_MANAGER, interop.DXGIManager() ) ) )
        {
            return true;
        }

        return false;
    }

    bool DXGIRenderPath::InitializeRenderTarget ( const ci::ivec2 & size )
    {
        if ( !_sharedTexture || size != _size )
        {
            _size = size;
            _sharedTexture = InteropContext::Get ( ).CreateSharedTexture ( size );
        }

        return ( _sharedTexture != nullptr );
    }

    bool DXGIRenderPath::ProcessFrame ( )
    {
        if ( _sharedTexture )
        {
            auto & engine = _owner._mediaEngine;

            MFVideoNormalizedRect srcRect{ 0.0f, 0.0f, 1.0f, 1.0f };
            RECT dstRect{ 0, 0, _size.x, _size.y };
            MFARGB black{ 0, 0, 0, 1 };

            bool ok = SUCCEEDED ( engine->TransferVideoFrame ( _sharedTexture->DXTextureHandle(), &srcRect, &dstRect, &black ) );
            if ( ok )
            {
                _owner._hasNewFrame.store ( true );
            }

            return ok;
        }

        return false;
    }

    MediaPlayer::FrameLeaseRef DXGIRenderPath::GetFrameLease ( ) const
    {
        return std::make_unique<DXGIRenderPathFrameLease> ( _sharedTexture );
    }

    DXGIRenderPath::~DXGIRenderPath ( )
    {
        
    }
}