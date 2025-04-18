//
//  AX-MediaPlayerMSWDXGIRenderPath.h
//  AX-MediaPlayer
//
//  Created by Andrew Wright (@axjxwright) on 17/08/21.
//  (c) 2021 AX Interactive (axinteractive.com.au)
//

#pragma once

#include "AX-MediaPlayerMSWImpl.h"
#include "cinder/gl/gl.h"
#include <d3d11.h>

namespace AX::Video
{
    class DXGIRenderPath : public MediaPlayer::Impl::RenderPath
    {
    public:

        class  SharedTexture;
        struct SharedTextureDeleter { void operator() ( SharedTexture* ) const; };
        using  SharedTextureRef     = std::unique_ptr<SharedTexture, SharedTextureDeleter>;

        DXGIRenderPath              ( MediaPlayer::Impl & owner, const ci::DataSourceRef & source );
        ~DXGIRenderPath             ( );
        
        bool Initialize             ( IMFAttributes & attributes ) override;
        bool InitializeRenderTarget ( const ci::ivec2 & size ) override;
        bool ProcessFrame           ( ) override;
        MediaPlayer::FrameLeaseRef GetFrameLease ( ) const override;
    
    protected:

        SharedTextureRef _sharedTexture{ nullptr };
    };
}