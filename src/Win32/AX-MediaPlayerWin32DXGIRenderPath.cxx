//
//  AX-MediaPlayerWin32DXGIRenderPath.cxx
//  AX-MediaPlayer
//
//  Created by Andrew Wright on 17/08/21.
//  (c) 2021 AX Interactive
//


#pragma comment(lib, "d3d11.lib")

#include "AX-MediaPlayerWin32DXGIRenderPath.h"
#include "glad/glad_wgl.h"

using namespace ci;

namespace AX::Video
{
    class DXGIRenderPath::InteropContext
    {
    public:

        InteropContext ( ID3D11Device& device );
        ~InteropContext ( );

        ID3D11Device &          Device ( ) const { return _device; }
        HANDLE                  Handle ( ) const { return _interopHandle; }
        
        SharedTextureRef        CreateSharedTexture ( const ivec2 & size );
        inline bool             IsValid ( ) const { return _isValid; }
        
    protected:

        ID3D11Device&           _device;
        HANDLE                  _interopHandle{ nullptr };
        bool                    _isValid{ false };
    };

    class DXGIRenderPath::SharedTexture
    {
    public:

        SharedTexture           ( InteropContext & context, const ivec2 & size );
        ~SharedTexture          ( );

        inline bool IsValid     ( ) const { return _isValid; }

        ID3D11Texture2D *       DXTextureHandle ( ) { return _dxTexture.Get( ); }

    protected:

        InteropContext &        _context;
        ci::gl::TextureRef      _glTexture;
        ComPtr<ID3D11Texture2D> _dxTexture{ nullptr };
        HANDLE                  _shareHandle{ nullptr };
        bool                    _isValid{ false };
    };

    DXGIRenderPath::InteropContext::InteropContext ( ID3D11Device & device )
    : _device ( device )
    {
        _interopHandle = wglDXOpenDeviceNV ( &device );
        _isValid = _interopHandle != nullptr;
    }

    DXGIRenderPath::SharedTextureRef DXGIRenderPath::InteropContext::CreateSharedTexture ( const ivec2 & size )
    {
        auto texture = std::make_unique<SharedTexture> ( *this, size );
        if ( texture->IsValid ( ) ) return std::move ( texture );

        return nullptr;
    }

    DXGIRenderPath::InteropContext::~InteropContext ( )
    {
        wglDXCloseDeviceNV ( _interopHandle );
        _interopHandle = nullptr;
    }

    DXGIRenderPath::SharedTexture::SharedTexture ( InteropContext& context, const ivec2 & size )
        : _context ( context )
    {
        D3D11_TEXTURE2D_DESC desc = {};
        desc.Width = size.x;
        desc.Height = size.y;
        desc.MipLevels = 1;
        desc.ArraySize = 1;
        desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
        desc.SampleDesc.Count = 1;
        desc.SampleDesc.Quality = 0;
        desc.BindFlags = D3D11_BIND_RENDER_TARGET;
        desc.Usage = D3D11_USAGE_DEFAULT; // @todo(andrew): Confirm this

        // @todo(andrew): Apparently ATI/Intel cards needs a shared handle / wglDXSetResourceShareHandleNV?
        // Verify this is still the case with DX11
        if ( SUCCEEDED ( context.Device().CreateTexture2D ( &desc, nullptr, _dxTexture.GetAddressOf ( ) ) ) )
        {
            gl::Texture::Format fmt;
            fmt.internalFormat ( GL_RGBA ).loadTopDown ( );
            
            _glTexture = gl::Texture::create ( size.x, size.y, fmt );
            _shareHandle = wglDXRegisterObjectNV ( context.Handle(), _dxTexture.Get(), _glTexture->getId(), GL_TEXTURE_2D, WGL_ACCESS_READ_ONLY_NV );
            _isValid = true;
        }
    }

    DXGIRenderPath::SharedTexture::~SharedTexture ( )
    {
        wglDXUnlockObjectsNV ( _context.Handle(), 1, &_shareHandle );
        wglDXUnregisterObjectNV ( _context.Handle ( ), _shareHandle );
    }

    DXGIRenderPath::DXGIRenderPath ( MediaPlayer::Impl & owner, const ci::DataSourceRef & source, uint32_t flags )
        : RenderPath ( owner, source, flags )
    { }

    bool DXGIRenderPath::Initialize ( IMFAttributes & attributes )
    {
        UINT deviceFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT | D3D11_CREATE_DEVICE_VIDEO_SUPPORT;

        #ifndef _NDEBUG
        deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
        #endif

        UINT resetToken { 0 };
        if ( !SUCCEEDED ( MFCreateDXGIDeviceManager ( &resetToken, _dxgiManager.GetAddressOf ( ) ) ) ) return false;
        if ( !SUCCEEDED ( D3D11CreateDevice ( nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, deviceFlags, nullptr, 0, D3D11_SDK_VERSION, _device.GetAddressOf ( ), nullptr, nullptr ) ) ) return false;
        
        ComPtr<ID3D10Multithread> multiThread{ nullptr };
        if ( SUCCEEDED ( _device->QueryInterface ( multiThread.GetAddressOf() ) ) )
        {
            multiThread->SetMultithreadProtected ( true );
        }
        else
        {
            return false;
        }
        
        if ( !SUCCEEDED ( _dxgiManager->ResetDevice ( _device.Get ( ), resetToken ) ) ) return false;
        if ( SUCCEEDED ( attributes.SetUnknown ( MF_MEDIA_ENGINE_DXGI_MANAGER, _dxgiManager.Get ( ) ) ) )
        {
            _interopContext = std::make_unique<InteropContext> ( *_device.Get ( ) );
            return _interopContext->IsValid ( );
        }
        return false;
    }

    bool DXGIRenderPath::InitializeRenderTarget ( const ci::ivec2 & size )
    {
        if ( !_sharedTexture || size != _size )
        {
            _size = size;

            if ( _interopContext )
            {
                _sharedTexture = _interopContext->CreateSharedTexture ( size );
                return _sharedTexture != nullptr;
            }
            else
            {
                return false;
            }
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

            return SUCCEEDED ( engine->TransferVideoFrame ( _sharedTexture->DXTextureHandle(), &srcRect, &dstRect, &black ) );
        }

        return false;
    }

    DXGIRenderPath::~DXGIRenderPath ( )
    {
        // @note(andrew): Make sure _sharedTexture is killed before _interopContext because
        // _sharedTexture relies on the share handle to unregister itself
        _sharedTexture = nullptr;
        _interopContext = nullptr;
    }
}