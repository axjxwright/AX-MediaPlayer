//
//  AX-MediaPlayerWin32DXGIRenderPath.h
//  AX-MediaPlayer
//
//  Created by Andrew Wright on 17/08/21.
//  (c) 2021 AX Interactive
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

        DXGIRenderPath ( MediaPlayer::Impl & owner, const ci::DataSourceRef & source, uint32_t flags );
        ~DXGIRenderPath ( );
        
        bool Initialize ( IMFAttributes & attributes ) override;
        bool ProcessFrame ( ) override;
        bool InitializeRenderTarget ( const ci::ivec2 & size ) override;
    
    protected:

        ComPtr<ID3D11Device>            _device{ nullptr };
        ComPtr<IMFDXGIDeviceManager>    _dxgiManager{ nullptr };
        
        class InteropContext;
        using InteropContextRef         = std::unique_ptr<InteropContext>;
        InteropContextRef               _interopContext{ nullptr };
        
        class SharedTexture;
        using SharedTextureRef          = std::unique_ptr<SharedTexture>;
        SharedTextureRef                _sharedTexture{ nullptr };
    };
}