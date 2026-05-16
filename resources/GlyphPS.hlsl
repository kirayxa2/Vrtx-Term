// GlyphPS.hlsl - Pixel shader for the GPU glyph atlas instanced draw.
//
// The atlas is A8_UNORM: the alpha channel encodes glyph coverage.
// We sample it and multiply by the per-instance foreground colour.

Texture2D<float>  gAtlas   : register(t1);
SamplerState      gSampler : register(s0);

struct PSIn {
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};

float4 main(PSIn i) : SV_TARGET {
    float coverage = gAtlas.Sample(gSampler, i.uv);
    // Pre-multiply alpha for the swap chain (DXGI_ALPHA_MODE_PREMULTIPLIED).
    float4 c = i.col * coverage;
    return float4(c.rgb * c.a, c.a);
}
