//
//  AX-MediaPlayerWin32DXGIRenderPath.cxx
//  AX-MediaPlayer
//
//  Created by Andrew Wright on 17/08/21.
//  (c) 2021 AX Interactive
//


#include "AX-MediaPlayerWin32DXGIRenderPath.h"

using namespace ci;

namespace AX::Video
{
    DXGIRenderPath::DXGIRenderPath ( MediaPlayer::Impl & owner, const ci::DataSourceRef & source, uint32_t flags )
        : RenderPath ( owner, source, flags )
    { }

    bool DXGIRenderPath::InitializeRenderTarget ( const ci::ivec2 & size )
    {
        _size = size;
        return false;
    }

    bool DXGIRenderPath::ProcessFrame ( )
    {

        return false;
    }
}