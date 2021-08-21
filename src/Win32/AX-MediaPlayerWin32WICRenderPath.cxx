//
//  AX-MediaPlayerWin32WICRenderPath.cxx
//  AX-MediaPlayer
//
//  Created by Andrew Wright on 17/08/21.
//  (c) 2021 AX Interactive
//

#include "AX-MediaPlayerWin32WICRenderPath.h"

using namespace ci;

namespace AX::Video
{
    WICRenderPath::WICRenderPath ( MediaPlayer::Impl & owner, const ci::DataSourceRef & source, uint32_t flags )
        : RenderPath ( owner, source, flags )
    {
        if ( SUCCEEDED ( CoCreateInstance ( CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS ( &_wicFactory ) ) ) )
        {
            // @todo(andrew): work out how best to flag an error like this
        }
    }
        
    bool WICRenderPath::InitializeRenderTarget ( const ci::ivec2 & size )
    {
        if ( !_wicBitmap || size != _size )
        {
            _size = size;

            if ( _wicFactory )
            {
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
            MFARGB black{ 0, 0, 0, 1 };

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
                            // @note(andrew): I believe constructing the Surface8u 
                            // this way forces it to copy the data into its own buffer
                            _owner._surface = Surface8u::create ( surface );
                            return true;
                        }
                    }
                }
            }
        }

        return false;
    }
}