#include "render/AtlasRenderer.h"

#include <d3dcompiler.h>
#include <string>

#pragma comment(lib, "d3dcompiler.lib")

namespace vrtx::render {

// ---------------------------------------------------------------------------
// Embedded HLSL source (same content as the .hlsl files; we compile at
// runtime so there is no separate build step).
// ---------------------------------------------------------------------------

static const char* kVsSrc = R"HLSL(
struct GlyphInstance {
    float destX, destY;
    float srcU0, srcV0;
    float srcU1, srcV1;
    float offX,  offY;
    float r, g, b, a;
};

StructuredBuffer<GlyphInstance> gInstances : register(t0);

cbuffer cbScene : register(b0) {
    float2 gViewportSize;
    float2 gOriginOffset;  // squircle origin in swap-chain px (marginPx, marginPx + captionPx)
};

struct VSOut {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

VSOut main(uint vertId : SV_VertexID, uint instId : SV_InstanceID) {
    GlyphInstance gi = gInstances[instId];

    float glyphW = (gi.srcU1 - gi.srcU0) * 2048.0f;
    float glyphH = (gi.srcV1 - gi.srcV0) * 2048.0f;

    float lx = gi.destX + gi.offX + gOriginOffset.x;
    float ty = gi.destY + gi.offY + gOriginOffset.y;
    float rx = lx + glyphW;
    float by = ty + glyphH;

    float2 corners[4] = {
        float2(lx, ty), float2(rx, ty),
        float2(lx, by), float2(rx, by),
    };
    float2 uvs[4] = {
        float2(gi.srcU0, gi.srcV0), float2(gi.srcU1, gi.srcV0),
        float2(gi.srcU0, gi.srcV1), float2(gi.srcU1, gi.srcV1),
    };

    float2 p = corners[vertId];
    VSOut o;
    o.pos = float4(
        p.x / gViewportSize.x *  2.0f - 1.0f,
        p.y / gViewportSize.y * -2.0f + 1.0f,
        0.0f, 1.0f);
    o.uv  = uvs[vertId];
    o.col = float4(gi.r, gi.g, gi.b, gi.a);
    return o;
}
)HLSL";

static const char* kPsSrc = R"HLSL(
Texture2D<float> gAtlas   : register(t1);
SamplerState     gSampler : register(s0);

struct PSIn {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

float4 main(PSIn i) : SV_TARGET {
    float cov = gAtlas.Sample(gSampler, i.uv);
    float4 c  = i.col * cov;
    return float4(c.rgb * c.a, c.a);
}
)HLSL";

// ---------------------------------------------------------------------------

bool AtlasRenderer::Initialize(ID3D11Device* device,
                                ID3D11DeviceContext* ctx) {
    dev_ = device;
    ctx_ = ctx;

    if (!CompileShaders()) return false;
    if (!CreatePipeline()) return false;

    ready_ = true;
    return true;
}

// ---------------------------------------------------------------------------

bool AtlasRenderer::CompileShaders() {
    ComPtr<ID3DBlob> vsBlob, psBlob, errBlob;

    HRESULT hr = ::D3DCompile(
        kVsSrc, strlen(kVsSrc),
        "GlyphVS", nullptr, nullptr,
        "main", "vs_4_0",
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        vsBlob.GetAddressOf(), errBlob.GetAddressOf());

    if (FAILED(hr)) {
        if (errBlob) {
            ::OutputDebugStringA(
                static_cast<const char*>(errBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = ::D3DCompile(
        kPsSrc, strlen(kPsSrc),
        "GlyphPS", nullptr, nullptr,
        "main", "ps_4_0",
        D3DCOMPILE_OPTIMIZATION_LEVEL3, 0,
        psBlob.GetAddressOf(), errBlob.GetAddressOf());

    if (FAILED(hr)) {
        if (errBlob) {
            ::OutputDebugStringA(
                static_cast<const char*>(errBlob->GetBufferPointer()));
        }
        return false;
    }

    hr = dev_->CreateVertexShader(
        vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(),
        nullptr, vs_.GetAddressOf());
    if (FAILED(hr)) return false;

    hr = dev_->CreatePixelShader(
        psBlob->GetBufferPointer(), psBlob->GetBufferSize(),
        nullptr, ps_.GetAddressOf());
    return SUCCEEDED(hr);
}

// ---------------------------------------------------------------------------

bool AtlasRenderer::CreatePipeline() {
    HRESULT hr;

    // ---- Scene cbuffer (viewport size) ------------------------------------
    D3D11_BUFFER_DESC cbd{};
    cbd.ByteWidth      = 16;   // float4 (float2 + pad)
    cbd.Usage          = D3D11_USAGE_DEFAULT;
    cbd.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
    hr = dev_->CreateBuffer(&cbd, nullptr, cbScene_.GetAddressOf());
    if (FAILED(hr)) return false;

    // ---- Sampler (linear, clamp) ------------------------------------------
    D3D11_SAMPLER_DESC sd{};
    sd.Filter         = D3D11_FILTER_MIN_MAG_LINEAR_MIP_POINT;
    sd.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
    sd.MaxAnisotropy  = 1;
    sd.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
    sd.MaxLOD         = D3D11_FLOAT32_MAX;
    hr = dev_->CreateSamplerState(&sd, sampler_.GetAddressOf());
    if (FAILED(hr)) return false;

    // ---- Blend state: pre-multiplied alpha-over ---------------------------
    D3D11_BLEND_DESC bd{};
    bd.RenderTarget[0].BlendEnable           = TRUE;
    bd.RenderTarget[0].SrcBlend              = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
    bd.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_INV_SRC_ALPHA;
    bd.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
    bd.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
    hr = dev_->CreateBlendState(&bd, blendState_.GetAddressOf());
    if (FAILED(hr)) return false;

    // ---- Depth-stencil: disabled ------------------------------------------
    D3D11_DEPTH_STENCIL_DESC dsd{};
    dsd.DepthEnable   = FALSE;
    dsd.StencilEnable = FALSE;
    hr = dev_->CreateDepthStencilState(&dsd, dsState_.GetAddressOf());
    if (FAILED(hr)) return false;

    // ---- Rasterizer: no culling -------------------------------------------
    D3D11_RASTERIZER_DESC rd{};
    rd.FillMode = D3D11_FILL_SOLID;
    rd.CullMode = D3D11_CULL_NONE;
    hr = dev_->CreateRasterizerState(&rd, rsState_.GetAddressOf());
    return SUCCEEDED(hr);
}

// ---------------------------------------------------------------------------

void AtlasRenderer::GrowInstanceBuffer(UINT needed) {
    if (needed <= instanceBufCap_) return;

    // Round up to the next power of two (minimum 256).
    UINT cap = std::max(256u, instanceBufCap_);
    while (cap < needed) cap *= 2;

    instanceBuf_.Reset();
    instanceSRV_.Reset();

    D3D11_BUFFER_DESC bd{};
    bd.ByteWidth           = static_cast<UINT>(sizeof(GlyphInstance)) * cap;
    bd.Usage               = D3D11_USAGE_DYNAMIC;
    bd.BindFlags           = D3D11_BIND_SHADER_RESOURCE;
    bd.CPUAccessFlags      = D3D11_CPU_ACCESS_WRITE;
    bd.MiscFlags           = D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
    bd.StructureByteStride = sizeof(GlyphInstance);

    if (FAILED(dev_->CreateBuffer(&bd, nullptr, instanceBuf_.GetAddressOf()))) {
        return;
    }

    D3D11_SHADER_RESOURCE_VIEW_DESC srvd{};
    srvd.Format              = DXGI_FORMAT_UNKNOWN;
    srvd.ViewDimension       = D3D11_SRV_DIMENSION_BUFFER;
    srvd.Buffer.FirstElement = 0;
    srvd.Buffer.NumElements  = cap;
    dev_->CreateShaderResourceView(
        instanceBuf_.Get(), &srvd, instanceSRV_.GetAddressOf());

    instanceBufCap_ = cap;
}

// ---------------------------------------------------------------------------

void AtlasRenderer::BeginFrame() {
    instances_.clear();
}

void AtlasRenderer::AddGlyph(const GlyphInstance& inst) {
    instances_.push_back(inst);
}

// ---------------------------------------------------------------------------

void AtlasRenderer::Flush(ID3D11DeviceContext* ctx,
                           ID3D11ShaderResourceView* atlasSRV,
                           float vpW, float vpH,
                           float originX, float originY) {
    if (!ready_ || instances_.empty()) return;

    const UINT count = static_cast<UINT>(instances_.size());

    // Upload instance data.
    GrowInstanceBuffer(count);
    if (!instanceBuf_) return;

    D3D11_MAPPED_SUBRESOURCE mapped{};
    if (SUCCEEDED(ctx->Map(instanceBuf_.Get(), 0,
                           D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
        memcpy(mapped.pData, instances_.data(),
               count * sizeof(GlyphInstance));
        ctx->Unmap(instanceBuf_.Get(), 0);
    }

    // Upload scene cbuffer.
    float cb[4] = {vpW, vpH, originX, originY};
    ctx->UpdateSubresource(cbScene_.Get(), 0, nullptr, cb, 0, 0);

    // Set pipeline state.
    ctx->VSSetShader(vs_.Get(), nullptr, 0);
    ctx->PSSetShader(ps_.Get(), nullptr, 0);

    ID3D11Buffer* cbs[] = {cbScene_.Get()};
    ctx->VSSetConstantBuffers(0, 1, cbs);

    ID3D11ShaderResourceView* vsSRVs[] = {instanceSRV_.Get()};
    ctx->VSSetShaderResources(0, 1, vsSRVs);

    ID3D11ShaderResourceView* psSRVs[] = {atlasSRV};
    ctx->PSSetShaderResources(1, 1, psSRVs);

    ID3D11SamplerState* samplers[] = {sampler_.Get()};
    ctx->PSSetSamplers(0, 1, samplers);

    const float blendFactor[4]{};
    ctx->OMSetBlendState(blendState_.Get(), blendFactor, 0xFFFFFFFF);
    ctx->OMSetDepthStencilState(dsState_.Get(), 0);
    ctx->RSSetState(rsState_.Get());

    ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
    ctx->IASetInputLayout(nullptr);

    // 4 vertices per quad, one quad per instance.
    ctx->DrawInstanced(4, count, 0, 0);

    // Unbind SRVs to avoid D3D debug warnings.
    ID3D11ShaderResourceView* nullSRV = nullptr;
    ctx->VSSetShaderResources(0, 1, &nullSRV);
    ctx->PSSetShaderResources(1, 1, &nullSRV);
}

}  // namespace vrtx::render
