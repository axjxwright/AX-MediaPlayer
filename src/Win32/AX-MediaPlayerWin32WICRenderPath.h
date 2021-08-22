//
//  AX-MediaPlayerWin32WICRenderPath.h
//  AX-MediaPlayer
//
//  Created by Andrew Wright (@axjxwright) on 17/08/21.
//  (c) 2021 AX Interactive (axinteractive.com.au)
//

#pragma once

#include "AX-MediaPlayerWin32Impl.h"

namespace AX::Video
{
    class WICRenderPath : public MediaPlayer::Impl::RenderPath
    {
    public:

        WICRenderPath ( MediaPlayer::Impl & owner, const ci::DataSourceRef & source, uint32_t flags );
        
        bool ProcessFrame ( ) override;
        bool InitializeRenderTarget ( const ci::ivec2 & size ) override;
        MediaPlayer::FrameLeaseRef GetFrameLease ( ) const override;
    
    protected:

        ComPtr<IWICBitmap> _wicBitmap{ nullptr };
        ComPtr<IWICImagingFactory> _wicFactory{ nullptr };
    };
}