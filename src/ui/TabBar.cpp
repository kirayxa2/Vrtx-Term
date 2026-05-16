#include "ui/TabBar.h"

#include "theme/AppTheme.h"

namespace vrtx::ui {

namespace {

// Tab pill metrics (logical pt).
//
// Slightly shorter than before so the strip reads as compact and the
// label/close-x have room to breathe without crowding the 28pt strip.
constexpr float kTabPillH       = 18.0f;   // pill height
constexpr float kTabMinW        = 80.0f;   // minimum pill width (room for label)
constexpr float kTabMaxW        = 180.0f;  // maximum pill width
constexpr float kTabGap         = 4.0f;    // gap between pills
constexpr float kTabFontSize    = 11.0f;   // label font size
constexpr float kPillRadius     = 5.0f;    // corner radius

// Close × button inside the pill (right side, macOS-Tahoe style).
//
// Smaller than the pill and inset toward the right edge. Clicking
// outside the close-rect (but inside the pill) just switches/activates
// the tab, so there's plenty of room for the label even at minimum
// pill width.
constexpr float kCloseSize      = 12.0f;   // hit area size
constexpr float kCloseMargin    =  3.0f;   // from right/vertical edge of pill

// "+" new-tab button metrics.
//
// The button hugs the right edge of the LAST pill (offset by kPlusGap),
// not the right edge of the strip. The pill row therefore "ends" with
// a small "+" affordance, just like Windows Terminal / macOS Terminal.
constexpr float kPlusSize       = 16.0f;   // circle diameter
constexpr float kPlusGap        =  6.0f;   // gap between last pill and "+"

}  // namespace

// ---------------------------------------------------------------------------

void TabBar::UpdateLayout(D2D1_RECT_F stripRect, UINT dpi) {
    strip_rect_ = stripRect;
    dpi_        = dpi;
    RebuildPills();
}

void TabBar::SetTabs(std::vector<Tab> tabs, int activeIdx) {
    tabs_       = std::move(tabs);
    active_idx_ = activeIdx;
    hover_idx_  = -1;
    hover_plus_ = false;
    pills_.resize(tabs_.size());
    for (int i = 0; i < static_cast<int>(pills_.size()); ++i) {
        pills_[i].active     = (i == active_idx_);
        pills_[i].hoverClose = false;
    }
    RebuildPills();
}

void TabBar::RebuildPills() {
    const int n = static_cast<int>(tabs_.size());
    pills_.resize(n);
    if (n == 0) {
        plus_rect_ = {};
        return;
    }

    const float gapPx     = theme::ToPx(kTabGap,    dpi_);
    const float minWPx    = theme::ToPx(kTabMinW,   dpi_);
    const float maxWPx    = theme::ToPx(kTabMaxW,   dpi_);
    const float pillHPx   = theme::ToPx(kTabPillH,  dpi_);
    const float plusSzPx  = theme::ToPx(kPlusSize,  dpi_);
    const float plusGapPx = theme::ToPx(kPlusGap,   dpi_);
    const float closeSzPx = theme::ToPx(kCloseSize, dpi_);
    const float closeMarPx= theme::ToPx(kCloseMargin, dpi_);
    const float padXPx    = theme::ToPx(theme::kTabStripPaddingX, dpi_);

    const float stripW = std::max(1.0f, strip_rect_.right - strip_rect_.left);
    const float stripH = std::max(1.0f, strip_rect_.bottom - strip_rect_.top);

    // Reserve space for the "+" button on the right and side padding.
    const float available  = stripW - 2.0f * padXPx - plusSzPx - plusGapPx;
    const float totalGaps  = gapPx * (n - 1);
    const float pillW      = std::clamp((available - totalGaps) / n,
                                        minWPx, maxWPx);

    // Pills are left-aligned inside the strip (macOS Terminal style).
    const float startX = strip_rect_.left + padXPx;
    const float pillY  = strip_rect_.top + (stripH - pillHPx) * 0.5f;

    for (int i = 0; i < n; ++i) {
        const float x0 = startX + i * (pillW + gapPx);
        pills_[i].rect   = {x0, pillY, x0 + pillW, pillY + pillHPx};
        pills_[i].active = (i == active_idx_);

        // Close button: vertically-centred against the pill, anchored
        // to the right edge with kCloseMargin breathing room. macOS
        // Terminal puts the close-x on the left of the active tab; we
        // ship the more familiar Chrome / Windows Terminal layout
        // (right-aligned x), so the label reads left-to-right with the
        // close affordance trailing.
        const float closeY = pills_[i].rect.top
                           + (pillHPx - closeSzPx) * 0.5f;
        pills_[i].closeRect = {
            pills_[i].rect.right - closeMarPx - closeSzPx,
            closeY,
            pills_[i].rect.right - closeMarPx,
            closeY + closeSzPx,
        };
    }

    // "+" button hugs the right edge of the LAST pill - it's a small
    // companion to the tab row, not a strip-wide control. Vertically
    // centred against the pill height so the geometry reads as a
    // single horizontal group.
    const float lastRight = pills_[n - 1].rect.right;
    const float plusY     = pillY + (pillHPx - plusSzPx) * 0.5f;
    plus_rect_ = {
        lastRight + plusGapPx,
        plusY,
        lastRight + plusGapPx + plusSzPx,
        plusY + plusSzPx,
    };
}

// ---------------------------------------------------------------------------

void TabBar::OnMouseMove(float x, float y) {
    hover_idx_  = -1;
    hover_plus_ = false;

    // Check "+" first.
    if (x >= plus_rect_.left && x < plus_rect_.right &&
        y >= plus_rect_.top  && y < plus_rect_.bottom) {
        hover_plus_ = true;
        return;
    }

    for (int i = 0; i < static_cast<int>(pills_.size()); ++i) {
        const auto& r = pills_[i].rect;
        if (x >= r.left && x < r.right && y >= r.top && y < r.bottom) {
            hover_idx_ = i;
            // Check close button inside this pill.
            const auto& cr = pills_[i].closeRect;
            pills_[i].hoverClose = (x >= cr.left && x < cr.right &&
                                    y >= cr.top  && y < cr.bottom);
            break;
        } else {
            pills_[i].hoverClose = false;
        }
    }
}

void TabBar::OnMouseLeave() {
    hover_idx_  = -1;
    hover_plus_ = false;
    for (auto& p : pills_) p.hoverClose = false;
}

bool TabBar::OnLButtonUp(float x, float y) {
    // "+" button.
    if (x >= plus_rect_.left && x < plus_rect_.right &&
        y >= plus_rect_.top  && y < plus_rect_.bottom) {
        if (on_new_tab_) on_new_tab_();
        return true;
    }

    for (int i = 0; i < static_cast<int>(pills_.size()); ++i) {
        const auto& r = pills_[i].rect;
        if (x < r.left || x >= r.right || y < r.top || y >= r.bottom) continue;

        // Close button inside pill (only on active or hovered).
        const auto& cr = pills_[i].closeRect;
        if (x >= cr.left && x < cr.right && y >= cr.top && y < cr.bottom) {
            if (on_close_) on_close_(i);
            return true;
        }

        // Switch to tab.
        if (i != active_idx_) {
            active_idx_ = i;
            for (auto& p : pills_) p.active = false;
            pills_[i].active = true;
            if (on_switch_) on_switch_(i);
        }
        return true;
    }
    return false;
}

bool TabBar::HitTest(float x, float y) const {
    if (x >= plus_rect_.left && x < plus_rect_.right &&
        y >= plus_rect_.top  && y < plus_rect_.bottom) {
        return true;
    }
    for (const auto& p : pills_) {
        if (x >= p.rect.left && x < p.rect.right &&
            y >= p.rect.top  && y < p.rect.bottom) {
            return true;
        }
    }
    return false;
}

// ---------------------------------------------------------------------------

void TabBar::Render(ID2D1DeviceContext* dc,
                    ID2D1SolidColorBrush* brush,
                    IDWriteFactory*       dwrite,
                    bool                  windowActive) const {
    if (pills_.empty()) return;

    // Lazily create text format. Left-aligned (label hugs the left edge
    // of the pill, close-x lives on the right) — this matches how the
    // tab pills "read" left-to-right and keeps the label visible even
    // when truncated.
    if (!fmt_ && dwrite) {
        const float fontPx = theme::ToPx(kTabFontSize, dpi_);
        dwrite->CreateTextFormat(
            theme::kTerminalFontFamily, nullptr,
            DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,
            fontPx, L"en-us",
            const_cast<TabBar*>(this)->fmt_.GetAddressOf());
        if (fmt_) {
            fmt_->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
            fmt_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_LEADING);
            fmt_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
            DWRITE_TRIMMING trim{DWRITE_TRIMMING_GRANULARITY_CHARACTER, 0, 0};
            fmt_->SetTrimming(&trim, nullptr);
        }
    }

    const float radius    = theme::ToPx(kPillRadius, dpi_);
    const float closeSzPx = theme::ToPx(kCloseSize,  dpi_);
    const float alpha     = windowActive ? 1.0f : 0.5f;

    // ---- Tab pills -------------------------------------------------------
    for (int i = 0; i < static_cast<int>(pills_.size()); ++i) {
        const auto& pill = pills_[i];
        const bool  hov  = (hover_idx_ == i);
        const bool  showClose = pill.active || hov;

        if (pill.active) {
            brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha * 0.13f));
            dc->FillRoundedRectangle(
                D2D1::RoundedRect(pill.rect, radius, radius), brush);
            brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha * 0.22f));
            dc->DrawRoundedRectangle(
                D2D1::RoundedRect(pill.rect, radius, radius), brush, 1.0f);
        } else if (hov) {
            brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha * 0.07f));
            dc->FillRoundedRectangle(
                D2D1::RoundedRect(pill.rect, radius, radius), brush);
            brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha * 0.13f));
            dc->DrawRoundedRectangle(
                D2D1::RoundedRect(pill.rect, radius, radius), brush, 1.0f);
        }

        // ---- Label (shrink rect to leave room for close button) ----------
        if (fmt_ && i < static_cast<int>(tabs_.size())) {
            // Higher base alpha than before so labels are readable on
            // top of the translucent strip background. The active pill
            // gets full opacity, hover sits a step below, idle pills
            // are dimmed but still legible.
            const float labelAlpha = pill.active ? alpha * 1.00f :
                                     hov         ? alpha * 0.85f :
                                                   alpha * 0.65f;
            brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, labelAlpha));

            // Shrink label rect so it doesn't overlap the × button
            // (close button is on the RIGHT of the pill). Left padding
            // keeps the leading character clear of the pill's rounded
            // corner; right padding leaves room for the close-x.
            D2D1_RECT_F labelRect = pill.rect;
            labelRect.left += theme::ToPx(8.0f, dpi_);
            if (showClose)
                labelRect.right -= closeSzPx + theme::ToPx(kCloseMargin, dpi_);
            else
                labelRect.right -= theme::ToPx(6.0f, dpi_);

            const auto& t = tabs_[i].title;
            dc->DrawTextW(t.c_str(), static_cast<UINT32>(t.size()),
                          fmt_.Get(), labelRect, brush,
                          D2D1_DRAW_TEXT_OPTIONS_CLIP |
                          D2D1_DRAW_TEXT_OPTIONS_ENABLE_COLOR_FONT);
        }

        // ---- Close × button (only on active or hovered tab) --------------
        if (showClose) {
            const float cx = (pill.closeRect.left + pill.closeRect.right)  * 0.5f;
            const float cy = (pill.closeRect.top  + pill.closeRect.bottom) * 0.5f;
            const float arm = closeSzPx * 0.28f;

            if (pill.hoverClose) {
                // Hover: draw a filled circle behind the ×.
                const float r2 = closeSzPx * 0.5f;
                D2D1_ELLIPSE ell{D2D1::Point2F(cx, cy), r2, r2};
                brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha * 0.18f));
                dc->FillEllipse(ell, brush);
            }

            const float xAlpha = pill.hoverClose ? alpha * 0.95f : alpha * 0.45f;
            brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, xAlpha));

            ComPtr<ID2D1PathGeometry> xPath;
            ComPtr<ID2D1Factory> xFactory;
            dc->GetFactory(xFactory.GetAddressOf());
            if (xFactory && SUCCEEDED(xFactory->CreatePathGeometry(xPath.GetAddressOf()))) {
                ComPtr<ID2D1GeometrySink> sink;
                xPath->Open(sink.GetAddressOf());
                sink->BeginFigure(D2D1::Point2F(cx - arm, cy - arm),
                                  D2D1_FIGURE_BEGIN_HOLLOW);
                sink->AddLine(D2D1::Point2F(cx + arm, cy + arm));
                sink->EndFigure(D2D1_FIGURE_END_OPEN);
                sink->BeginFigure(D2D1::Point2F(cx + arm, cy - arm),
                                  D2D1_FIGURE_BEGIN_HOLLOW);
                sink->AddLine(D2D1::Point2F(cx - arm, cy + arm));
                sink->EndFigure(D2D1_FIGURE_END_OPEN);
                sink->Close();

                ComPtr<ID2D1StrokeStyle> ss;
                D2D1_STROKE_STYLE_PROPERTIES sp{};
                sp.startCap = D2D1_CAP_STYLE_ROUND;
                sp.endCap   = D2D1_CAP_STYLE_ROUND;
                xFactory->CreateStrokeStyle(sp, nullptr, 0,
                                            ss.GetAddressOf());
                dc->DrawGeometry(xPath.Get(), brush, 1.5f, ss.Get());
            }
        }
    }

    // ---- "+" new-tab button --------------------------------------------
    {
        const float cx = (plus_rect_.left + plus_rect_.right)  * 0.5f;
        const float cy = (plus_rect_.top  + plus_rect_.bottom) * 0.5f;
        const float r2 = theme::ToPx(kPlusSize, dpi_) * 0.5f;
        const float arm = r2 * 0.45f;

        if (hover_plus_) {
            D2D1_ELLIPSE ell{D2D1::Point2F(cx, cy), r2, r2};
            brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha * 0.12f));
            dc->FillEllipse(ell, brush);
            brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, alpha * 0.20f));
            dc->DrawEllipse(ell, brush, 1.0f);
        }

        // Draw "+" cross.
        const float plusAlpha = hover_plus_ ? alpha * 0.90f : alpha * 0.40f;
        brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, plusAlpha));

        ComPtr<ID2D1PathGeometry> plusPath;
        ComPtr<ID2D1Factory> plusFactory;
        dc->GetFactory(plusFactory.GetAddressOf());
        if (plusFactory && SUCCEEDED(plusFactory->CreatePathGeometry(plusPath.GetAddressOf()))) {
            ComPtr<ID2D1GeometrySink> sink;
            plusPath->Open(sink.GetAddressOf());
            // horizontal bar
            sink->BeginFigure(D2D1::Point2F(cx - arm, cy),
                              D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddLine(D2D1::Point2F(cx + arm, cy));
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            // vertical bar
            sink->BeginFigure(D2D1::Point2F(cx, cy - arm),
                              D2D1_FIGURE_BEGIN_HOLLOW);
            sink->AddLine(D2D1::Point2F(cx, cy + arm));
            sink->EndFigure(D2D1_FIGURE_END_OPEN);
            sink->Close();

            ComPtr<ID2D1StrokeStyle> ss;
            D2D1_STROKE_STYLE_PROPERTIES sp{};
            sp.startCap = D2D1_CAP_STYLE_ROUND;
            sp.endCap   = D2D1_CAP_STYLE_ROUND;
            plusFactory->CreateStrokeStyle(sp, nullptr, 0,
                                           ss.GetAddressOf());
            dc->DrawGeometry(plusPath.Get(), brush, 1.5f, ss.Get());
        }
    }
}

}  // namespace vrtx::ui
