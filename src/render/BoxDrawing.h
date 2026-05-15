// Box-drawing glyph renderer.
//
// The glyphs in U+2500..U+259F (Unicode "Box Drawing" + "Block Elements")
// have a particularly nasty rendering property: they are supposed to tile
// seamlessly with their neighbours, but every font ships them at a
// specific intrinsic size and most fonts render them with a few pixels
// of padding above/below. As soon as you draw two `│` glyphs on
// consecutive rows with line-height > 1.0, the padding becomes a visible
// gap between the strokes.
//
// alacritty/kitty/Windows Terminal solve this by rendering the box and
// block characters from code rather than the font: each glyph becomes a
// few rectangles drawn from the cell centre out to its edges. Because
// adjacent cells draw their own halves out to the shared edge, the
// strokes always meet pixel-perfect regardless of line-height.
//
// We support:
//   * Single + heavy line drawing (U+2500..U+251B, U+251C, U+2524,
//     U+252C, U+2534, U+253C). Enough for grml-zsh, oh-my-posh and
//     similar shell prompts.
//   * Block elements U+2580..U+258F (full block, halves, eighths). Used
//     by neofetch, htop, lazygit progress bars, etc.
//
// Anything outside these ranges (rounded corners U+256D..U+2570, double
// lines U+2550..U+256C, shading U+2591..U+2593, ...) falls through to
// the regular DirectWrite path until someone needs them.

#pragma once

#include "pch.h"

namespace mactw::render::box {

// Returns true for codepoints we render ourselves.
bool IsBoxDrawing(char32_t cp);
bool IsBlockElement(char32_t cp);

// Draw the glyph for `cp` into `cell` using FillRectangle calls on `dc`
// with `brush`. `lightPx` is the stroke thickness for "light" lines,
// `heavyPx` for "heavy". Caller is responsible for setting brush colour
// before calling.
//
// If `cp` is not a recognised box-drawing / block character the function
// returns false and draws nothing - caller must fall back to DrawTextW.
bool DrawGlyph(ID2D1DeviceContext* dc,
               ID2D1SolidColorBrush* brush,
               D2D1_RECT_F cell,
               char32_t  cp,
               float     lightPx,
               float     heavyPx);

}  // namespace mactw::render::box
