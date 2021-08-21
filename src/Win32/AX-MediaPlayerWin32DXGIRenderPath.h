//
//  AX-MediaPlayerWin32DXGIRenderPath.h
//  AX-MediaPlayer
//
//  Created by Andrew Wright (@axjxwright) on 17/08/21.
//  (c) 2021 AX Interactive (axinteractive.com.au)
//

#pragma once

#include "AX-MediaPlayerWin32Impl.h"
#include "cinder/gl/gl.h"
#include <d3d11.h>

namespace AX::Video
{
    class DXGIRenderPath : public MediaPlayer::Impl::RenderPath
    {
    public:

        class SharedTexture;
        using SharedTextureRef = std::unique_ptr<SharedTexture>;

        DXGIRenderPath              ( MediaPlayer::Impl & owner, const ci::DataSourceRef & source, uint32_t flags );
        ~DXGIRenderPath             ( );
        
        bool Initialize             ( IMFAttributes & attributes ) override;
        bool InitializeRenderTarget ( const ci::ivec2 & size ) override;
        bool ProcessFrame           ( ) override;
        MediaPlayer::FrameLeaseRef GetFrameLease ( ) const override;
    
    protected:

        SharedTextureRef _sharedTexture{ nullptr };
    };
}