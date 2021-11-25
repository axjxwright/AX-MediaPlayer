//
//  AX-MediaPlayerMSWWICRenderPath.cxx
//  AX-MediaPlayer
//
//  Created by Andrew Wright on 17/08/21.
//  (c) 2021 AX Interactive
//

#include "AX-MediaPlayerMSWWICRenderPath.h"

using namespace ci;

namespace AX::Video
{
    class WICRenderPathFrameLease : public MediaPlayer::FrameLease
    {
    public:

        WICRenderPathFrameLease ( const Surface8uRef& surface )
        {
            if ( surface )
            {
                _texture = gl::Texture::create ( *surface, gl::Texture::Format ( ).loadTopDown ( ) );
            }
        }

        inline bool    IsValid ( ) const override { return ToTexture ( ) != nullptr; }
        gl::TextureRef ToTexture ( ) const override { return _texture; };

    protected:

        gl::TextureRef _texture{ nullptr };
    };

    WICRenderPath::WICRenderPath ( MediaPlayer::Impl & owner, const ci::DataSourceRef & source )
        : RenderPath ( owner, source )
    {
        if ( SUCCEEDED ( CoCreateInstance ( CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS ( &_wicFactory ) ) ) ) 
        {
           
        }
    }
        
    bool WICRenderPath::InitializeRenderTarget ( const ci::ivec2 & size )
    {
        if ( !_wicFactory ) return false;
        if ( !_wicBitmap || size != _size )
        {
            _size = size;

            if ( _wicFactory )
            {
                _owner._surface = Surface8u::create ( size.x, size.y, true, SurfaceChannelOrder::BGRA );
                return SUCCEEDED ( _wicFactory->CreateBitmap ( size.x, size.y, GUID_WICPixelFormat32bppBGRA, WICBitmapCacheOnDemand, _wicBitmap.GetAddressOf ( ) ) );
            }
            else
            {
                return false;
            }
        }

        return ( _wicBitmap != nullptr );
    }

    bool WICRenderPath::ProcessFrame ( )
    {
        auto& engine = _owner._mediaEngine;
        if ( _wicBitmap )
        {
            MFVideoNormalizedRect srcRect{ 0.0f, 0.0f, 1.0f, 1.0f };
            RECT dstRect{ 0, 0, _size.x, _size.y };
            MFARGB black{ 0, 0, 0, 0 };

            if ( SUCCEEDED ( engine->TransferVideoFrame ( _wicBitmap.Get ( ), &srcRect, &dstRect, &black ) ) )
            {
                ComPtr<IWICBitmapLock> lockedData;
                DWORD flags = WICBitmapLockRead;
                WICRect srcRect{ 0, 0, _size.x, _size.y };

                if ( SUCCEEDED ( _wicBitmap->Lock ( &srcRect, flags, lockedData.GetAddressOf ( ) ) ) )
                {
                    UINT stride{ 0 };

                    if ( SUCCEEDED ( lockedData->GetStride ( &stride ) ) )
                    {
                        UINT bufferSize{ 0 };
                        BYTE * data{ nullptr };

                        if ( SUCCEEDED ( lockedData->GetDataPointer ( &bufferSize, &data ) ) )
                        {
                            Surface8u surface ( data, _size.x, _size.y, stride, SurfaceChannelOrder::BGRA );
                            assert ( _owner._surface->getSize ( ) == surface.getSize ( ) );

                            _owner._surface->copyFrom ( surface, surface.getBounds ( ) );
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }

    MediaPlayer::FrameLeaseRef WICRenderPath::GetFrameLease ( ) const
    {
        return std::make_unique<WICRenderPathFrameLease> ( _owner._surface );
    }
}