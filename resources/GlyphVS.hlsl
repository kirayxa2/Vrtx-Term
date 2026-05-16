// GlyphVS.hlsl - Vertex shader for the GPU glyph atlas instanced draw.
//
// Each instance represents one on-screen glyph cell.
// We use SV_VertexID (0..3) to generate the four corners of a quad,
// combined with per-instance data from a structured buffer.
//
// The structured buffer is bound as t0 (SRV).
// cbScene holds the viewport dimensions for NDC conversion.

struct GlyphInstance {
    float destX, destY;    // cell top-left in squircle-local px
    float srcU0, srcV0;    // atlas UV top-left
    float srcU1, srcV1;    // atlas UV bot-right
    float offX,  offY;     // bearing offset inside the cell
    float r, g, b, a;      // foreground colour
};

StructuredBuffer<GlyphInstance> gInstances : register(t0);

cbuffer cbScene : register(b0) {
    float2 gViewportSize;   // (width, height) in physical px
    float2 _pad;
};

struct VSOut {
    float4 pos  : SV_POSITION;
    float2 uv   : TEXCOORD0;
    float4 col  : COLOR0;
};

VSOut main(uint vertId : SV_VertexID, uint instId : SV_InstanceID) {
    GlyphInstance gi = gInstances[instId];

    // Quad corners: 0=TL, 1=TR, 2=BL, 3=BR  (triangle strip order)
    // VertexID mapping:
    //   0 -> TL
    //   1 -> TR
    //   2 -> BL
    //   3 -> BR
    const float glyphW = (gi.srcU1 - gi.srcU0) * 2048.0f;  // atlas px
    const float glyphH = (gi.srcV1 - gi.srcV0) * 2048.0f;

    float lx = gi.destX + gi.offX;
    float ty = gi.destY + gi.offY;
    float rx = lx + glyphW;
    float by = ty + glyphH;

    float2 corners[4] = {
        float2(lx, ty),
        float2(rx, ty),
        float2(lx, by),
        float2(rx, by),
    };

    float2 uvs[4] = {
        float2(gi.srcU0, gi.srcV0),
        float2(gi.srcU1, gi.srcV0),
        float2(gi.srcU0, gi.srcV1),
        float2(gi.srcU1, gi.srcV1),
    };

    float2 p = corners[vertId];

    VSOut o;
    // Convert from squircle-local px to NDC.  Origin is top-left.
    o.pos  = float4(
        p.x / gViewportSize.x *  2.0f - 1.0f,
        p.y / gViewportSize.y * -2.0f + 1.0f,
        0.0f, 1.0f);
    o.uv   = uvs[vertId];
    o.col  = float4(gi.r, gi.g, gi.b, gi.a);
    return o;
}
