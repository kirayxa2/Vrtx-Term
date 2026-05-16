#include "render/BoxDrawing.h"

namespace mactw::render::box {

namespace {

// Each box-drawing glyph is described by 4 "legs" pointing N/E/S/W. Each
// leg is either absent (0), light (1), or heavy (2). The renderer draws
// one rectangle per non-absent leg, from the cell centre out to the
// matching cell edge. Adjacent cells produce mirrored half-strokes that
// meet pixel-perfect at the cell boundary, so joins are seamless even
// when the line-height is > 1.0.
//
// Codepoint reference (Unicode "Box Drawing" block, partial):
//
//   U+2500 ─   U+2501 ━     horizontals (light / heavy)
//   U+2502 │   U+2503 ┃     verticals
//   U+250C ┌   U+250D ┍   U+250E ┎   U+250F ┏   top-left corners
//   U+2510 ┐   U+2511 ┑   U+2512 ┒   U+2513 ┓   top-right corners
//   U+2514 └   U+2515 ┕   U+2516 ┖   U+2517 ┗   bottom-left corners
//   U+2518 ┘   U+2519 ┙   U+251A ┚   U+251B ┛   bottom-right corners
//   U+251C ├   U+2524 ┤   T-junctions (right, left)
//   U+252C ┬   U+2534 ┴   T-junctions (down, up)
//   U+253C ┼   cross
//   U+256D ╭   U+256E ╮   U+256F ╯   U+2570 ╰   rounded corners
//
// Naming convention for the table below: each entry's four numbers are
// (n, e, s, w), each in {0, 1, 2} for absent / light / heavy.
struct Legs { uint8_t n, e, s, w; };

constexpr Legs kNone{0, 0, 0, 0};

Legs LookupLegs(char32_t cp) {
    switch (cp) {
        // Horizontals
        case 0x2500: return {0, 1, 0, 1};  // ─
        case 0x2501: return {0, 2, 0, 2};  // ━

        // Verticals
        case 0x2502: return {1, 0, 1, 0};  // │
        case 0x2503: return {2, 0, 2, 0};  // ┃

        // Top-left corners
        case 0x250C: return {0, 1, 1, 0};  // ┌
        case 0x250D: return {0, 2, 1, 0};  // ┍
        case 0x250E: return {0, 1, 2, 0};  // ┎
        case 0x250F: return {0, 2, 2, 0};  // ┏

        // Top-right corners
        case 0x2510: return {0, 0, 1, 1};  // ┐
        case 0x2511: return {0, 0, 1, 2};  // ┑
        case 0x2512: return {0, 0, 2, 1};  // ┒
        case 0x2513: return {0, 0, 2, 2};  // ┓

        // Bottom-left corners
        case 0x2514: return {1, 1, 0, 0};  // └
        case 0x2515: return {1, 2, 0, 0};  // ┕
        case 0x2516: return {2, 1, 0, 0};  // ┖
        case 0x2517: return {2, 2, 0, 0};  // ┗

        // Bottom-right corners
        case 0x2518: return {1, 0, 0, 1};  // ┘
        case 0x2519: return {1, 0, 0, 2};  // ┙
        case 0x251A: return {2, 0, 0, 1};  // ┚
        case 0x251B: return {2, 0, 0, 2};  // ┛

        // T-junction: right (├ family)
        case 0x251C: return {1, 1, 1, 0};  // ├
        case 0x251D: return {1, 2, 1, 0};  // ┝
        case 0x2520: return {2, 1, 2, 0};  // ┠
        case 0x2523: return {2, 2, 2, 0};  // ┣

        // T-junction: left (┤ family)
        case 0x2524: return {1, 0, 1, 1};  // ┤
        case 0x2525: return {1, 0, 1, 2};  // ┥
        case 0x2528: return {2, 0, 2, 1};  // ┨
        case 0x252B: return {2, 0, 2, 2};  // ┫

        // T-junction: down (┬ family)
        case 0x252C: return {0, 1, 1, 1};  // ┬
        case 0x252D: return {0, 1, 1, 2};  // ┭
        case 0x252E: return {0, 2, 1, 1};  // ┮
        case 0x252F: return {0, 2, 1, 2};  // ┯
        case 0x2530: return {0, 1, 2, 1};  // ┰
        case 0x2533: return {0, 2, 2, 2};  // ┳

        // T-junction: up (┴ family)
        case 0x2534: return {1, 1, 0, 1};  // ┴
        case 0x2535: return {1, 1, 0, 2};  // ┵
        case 0x2536: return {1, 2, 0, 1};  // ┶
        case 0x2537: return {1, 2, 0, 2};  // ┷
        case 0x2538: return {2, 1, 0, 1};  // ┸
        case 0x253B: return {2, 2, 0, 2};  // ┻

        // Crosses
        case 0x253C: return {1, 1, 1, 1};  // ┼
        case 0x253D: return {1, 1, 1, 2};  // ┽
        case 0x253E: return {1, 2, 1, 1};  // ┾
        case 0x253F: return {1, 2, 1, 2};  // ┿
        case 0x2540: return {2, 1, 1, 1};  // ╀
        case 0x2541: return {1, 1, 2, 1};  // ╁
        case 0x2542: return {2, 1, 2, 1};  // ╂
        case 0x254B: return {2, 2, 2, 2};  // ╋

        default: return kNone;
    }
}

float StrokePx(uint8_t leg, float lightPx, float heavyPx) {
    switch (leg) {
        case 1:  return lightPx;
        case 2:  return heavyPx;
        default: return 0.0f;
    }
}

// Block elements: solid rectangles covering some fraction of the cell.
// Returns false for codepoints not in the U+2580..U+258F range.
bool DrawBlock(ID2D1DeviceContext* dc,
               ID2D1SolidColorBrush* brush,
               D2D1_RECT_F cell,
               char32_t cp) {
    if (cp < 0x2580 || cp > 0x258F) return false;

    const float w = cell.right  - cell.left;
    const float h = cell.bottom - cell.top;
    D2D1_RECT_F r = cell;

    switch (cp) {
        case 0x2580: r.bottom = cell.top + h * 0.5f;          break; // ▀
        case 0x2581: r.top    = cell.bottom - h * (1.0f/8);   break; // ▁
        case 0x2582: r.top    = cell.bottom - h * (2.0f/8);   break; // ▂
        case 0x2583: r.top    = cell.bottom - h * (3.0f/8);   break; // ▃
        case 0x2584: r.top    = cell.bottom - h * 0.5f;       break; // ▄
        case 0x2585: r.top    = cell.bottom - h * (5.0f/8);   break; // ▅
        case 0x2586: r.top    = cell.bottom - h * (6.0f/8);   break; // ▆
        case 0x2587: r.top    = cell.bottom - h * (7.0f/8);   break; // ▇
        case 0x2588: /* full block - keep `r == cell` */     break; // █
        case 0x2589: r.right  = cell.left + w * (7.0f/8);     break; // ▉
        case 0x258A: r.right  = cell.left + w * (6.0f/8);     break; // ▊
        case 0x258B: r.right  = cell.left + w * (5.0f/8);     break; // ▋
        case 0x258C: r.right  = cell.left + w * 0.5f;         break; // ▌
        case 0x258D: r.right  = cell.left + w * (3.0f/8);     break; // ▍
        case 0x258E: r.right  = cell.left + w * (2.0f/8);     break; // ▎
        case 0x258F: r.right  = cell.left + w * (1.0f/8);     break; // ▏
        default: return false;
    }
    dc->FillRectangle(r, brush);
    return true;
}

// Rounded corners: U+256D..U+2570. Drawn as a stroked path:
// "leg straight to start of arc -> cubic-bezier quarter-circle -> leg
// straight to opposite cell edge". We use a cubic Bezier with the
// classic K = 4*(sqrt(2)-1)/3 ~= 0.5523 control-point offset, which
// approximates a true quarter-circle to within ~0.02% error and
// removes any ambiguity about sweep direction (a Bezier's shape is
// fully determined by its 4 control points).
//
// Stroke width = lightPx so the curve joins pixel-perfect with
// neighbouring straight box cells of the same weight.
bool DrawRoundedCorner(ID2D1DeviceContext* dc,
                       ID2D1SolidColorBrush* brush,
                       D2D1_RECT_F cell,
                       char32_t cp,
                       float lightPx) {
    if (cp < 0x256D || cp > 0x2570) return false;

    ComPtr<ID2D1Factory> factory;
    dc->GetFactory(factory.GetAddressOf());
    if (!factory) return false;

    ComPtr<ID2D1PathGeometry> path;
    if (FAILED(factory->CreatePathGeometry(path.GetAddressOf()))) return false;

    ComPtr<ID2D1GeometrySink> sink;
    if (FAILED(path->Open(sink.GetAddressOf()))) return false;

    const float cx = (cell.left + cell.right)  * 0.5f;
    const float cy = (cell.top  + cell.bottom) * 0.5f;
    // Curve radius ~ 45% of the smaller half-cell. Smaller than before
    // (was 70%) so the rounding is visible but not exaggerated, which
    // matches how kitty/alacritty/iTerm draw these glyphs.
    const float halfMin = std::min(cell.right - cx, cell.bottom - cy);
    const float R = std::max(2.0f, halfMin * 0.45f);

    // Bezier offset for a 90 deg arc of radius R.
    constexpr float kQuarterCircleK = 0.5522847498307933f;
    const float K = R * kQuarterCircleK;

    D2D1_POINT_2F start{}, p0{}, p1{}, p2{}, p3{}, finish{};

    switch (cp) {
        case 0x256D: {  // ╭  east leg + south leg, curve bulging toward top-left
            // East leg: cell.right -> (cx+R, cy)
            // Arc: (cx+R, cy) -> (cx, cy+R), tangents west then south
            // South leg: (cx, cy+R) -> (cx, cell.bottom)
            start  = {cell.right, cy};
            p0     = {cx + R,     cy};
            p1     = {cx + R - K, cy};         // pull west
            p2     = {cx,         cy + R - K}; // pull up (arrive going south)
            p3     = {cx,         cy + R};
            finish = {cx,         cell.bottom};
            break;
        }
        case 0x256E: {  // ╮  west leg + south leg, curve bulging toward top-right
            start  = {cell.left,  cy};
            p0     = {cx - R,     cy};
            p1     = {cx - R + K, cy};         // pull east
            p2     = {cx,         cy + R - K}; // pull up
            p3     = {cx,         cy + R};
            finish = {cx,         cell.bottom};
            break;
        }
        case 0x256F: {  // ╯  west leg + north leg, curve bulging toward bottom-right
            start  = {cell.left,  cy};
            p0     = {cx - R,     cy};
            p1     = {cx - R + K, cy};         // pull east
            p2     = {cx,         cy - R + K}; // pull down (arrive going north)
            p3     = {cx,         cy - R};
            finish = {cx,         cell.top};
            break;
        }
        case 0x2570: {  // ╰  east leg + north leg, curve bulging toward bottom-left
            start  = {cell.right, cy};
            p0     = {cx + R,     cy};
            p1     = {cx + R - K, cy};         // pull west
            p2     = {cx,         cy - R + K}; // pull down
            p3     = {cx,         cy - R};
            finish = {cx,         cell.top};
            break;
        }
        default:
            sink->Close();
            return false;
    }

    sink->BeginFigure(start, D2D1_FIGURE_BEGIN_HOLLOW);
    sink->AddLine(p0);
    sink->AddBezier(D2D1::BezierSegment(p1, p2, p3));
    sink->AddLine(finish);
    sink->EndFigure(D2D1_FIGURE_END_OPEN);
    sink->Close();

    dc->DrawGeometry(path.Get(), brush, lightPx);
    return true;
}

}  // namespace

// ---------------------------------------------------------------------------

bool IsBoxDrawing(char32_t cp) {
    return cp >= 0x2500 && cp <= 0x257F;
}

bool IsBlockElement(char32_t cp) {
    return cp >= 0x2580 && cp <= 0x259F;
}

bool DrawGlyph(ID2D1DeviceContext* dc,
               ID2D1SolidColorBrush* brush,
               D2D1_RECT_F cell,
               char32_t cp,
               float lightPx,
               float heavyPx) {
    if (DrawBlock(dc, brush, cell, cp)) return true;
    if (DrawRoundedCorner(dc, brush, cell, cp, lightPx)) return true;

    const Legs g = LookupLegs(cp);
    if (g.n == 0 && g.e == 0 && g.s == 0 && g.w == 0) {
        return false;  // Not a recognised glyph; caller falls back to text.
    }

    const float cx = (cell.left + cell.right)  * 0.5f;
    const float cy = (cell.top  + cell.bottom) * 0.5f;

    // Each leg is a centred rect from the cell centre out to its edge.
    // The rect's perpendicular width is the stroke thickness; the
    // parallel length runs from the centre line to the edge plus a
    // half-stroke overhang so the leg fully covers the centre square at
    // joins (otherwise crosses leave a 1px hole).
    if (g.n) {
        const float t = StrokePx(g.n, lightPx, heavyPx);
        D2D1_RECT_F r{cx - t * 0.5f, cell.top, cx + t * 0.5f, cy + t * 0.5f};
        dc->FillRectangle(r, brush);
    }
    if (g.s) {
        const float t = StrokePx(g.s, lightPx, heavyPx);
        D2D1_RECT_F r{cx - t * 0.5f, cy - t * 0.5f, cx + t * 0.5f, cell.bottom};
        dc->FillRectangle(r, brush);
    }
    if (g.w) {
        const float t = StrokePx(g.w, lightPx, heavyPx);
        D2D1_RECT_F r{cell.left, cy - t * 0.5f, cx + t * 0.5f, cy + t * 0.5f};
        dc->FillRectangle(r, brush);
    }
    if (g.e) {
        const float t = StrokePx(g.e, lightPx, heavyPx);
        D2D1_RECT_F r{cx - t * 0.5f, cy - t * 0.5f, cell.right, cy + t * 0.5f};
        dc->FillRectangle(r, brush);
    }

    return true;
}

}  // namespace mactw::render::box
