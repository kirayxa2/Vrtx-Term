// AtlasRenderer - owns the D3D11 pipeline for instanced glyph drawing.
//
// Usage per frame:
//   1. BeginFrame()                    - clear the instance list
//   2. AddGlyph() for each visible cell
//   3. Flush(dc)                       - upload instance buffer + draw
//
// The shaders are compiled at runtime via D3DCompile so there is no
// build step for .hlsl -> .cso.  Compile-once, cache the blobs.

#pragma once

#include "pch.h"
#include "render/GlyphAtlas.h"
#include <vector>
#include <d3dcompiler.h>

namespace vrtx::render {

class AtlasRenderer {
public:
    AtlasRenderer()  = default;
    ~AtlasRenderer() = default;

    AtlasRenderer(const AtlasRenderer&)            = delete;
    AtlasRenderer& operator=(const AtlasRenderer&) = delete;

    // One-time setup.  Compiles shaders, creates pipeline state.
    // Returns false if shader compilation fails (rare; logs to debug output).
    bool Initialize(ID3D11Device* device, ID3D11DeviceContext* ctx);

    // Call at the start of each frame to reset the instance list.
    void BeginFrame();

    // Enqueue one glyph instance.
    void AddGlyph(const GlyphInstance& inst);

    // Upload all enqueued instances and issue one instanced draw call.
    // `renderTargetWidth` and `renderTargetHeight` are in physical px
    // (the swap-chain dimensions) so the VS can convert to NDC.
    void Flush(ID3D11DeviceContext* ctx,
               ID3D11ShaderResourceView* atlasSRV,
               float renderTargetWidth,
               float renderTargetHeight,
               float originX = 0.0f,
               float originY = 0.0f);

    bool Ready() const { return ready_; }

private:
    bool CompileShaders();
    bool CreatePipeline();
    void GrowInstanceBuffer(UINT needed);

    ComPtr<ID3D11Device>          dev_;
    ComPtr<ID3D11DeviceContext>   ctx_;

    // Shaders
    ComPtr<ID3D11VertexShader>    vs_;
    ComPtr<ID3D11PixelShader>     ps_;

    // No input layout needed: VS reads from a StructuredBuffer<GlyphInstance>
    // bound as SRV t0, indexed by SV_InstanceID.

    // Instance data: written CPU-side each frame, bound as t0.
    ComPtr<ID3D11Buffer>             instanceBuf_;
    ComPtr<ID3D11ShaderResourceView> instanceSRV_;
    UINT                             instanceBufCap_{0};

    // Scene cbuffer (viewport size) bound as b0.
    ComPtr<ID3D11Buffer> cbScene_;

    // Sampler for the atlas (linear, clamp).
    ComPtr<ID3D11SamplerState> sampler_;

    // Blend state: standard alpha-over for pre-multiplied alpha.
    ComPtr<ID3D11BlendState> blendState_;

    // Depth-stencil: disabled (2D UI).
    ComPtr<ID3D11DepthStencilState> dsState_;

    // Rasterizer: no culling, scissor enabled.
    ComPtr<ID3D11RasterizerState> rsState_;

    std::vector<GlyphInstance> instances_;
    bool ready_{false};
};

}  // namespace vrtx::render
