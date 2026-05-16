#include "ui/TabBar.h"

#include "theme/AppTheme.h"

namespace vrtx::ui {

namespace {

// Tab pill metrics (logical pt).
constexpr float kTabPillH       = 20.0f;   // pill height
constexpr float kTabMinW        = 48.0f;   // minimum pill width
constexpr float kTabMaxW        = 160.0f;  // maximum pill width
constexpr float kTabGap         = 4.0f;    // gap between pills
constexpr float kTabFontSize    = 11.5f;   // label font size
constexpr float kPillRadius     = 6.0f;    // corner radius

// Close × button inside the pill.
constexpr float kCloseSize      = 14.0f;   // hit area size
constexpr float kCloseMargin    =  3.0f;   // from right/top edge of pill

// "+" new-tab button metrics.
constexpr float kPlusSize       = 18.0f;   // circle diameter
constexpr float kPlusGap        =  5.0f;   // gap between last pill and "+"

}  // namespace

// ---------------------------------------------------------------------------

void TabBar::UpdateLayout(float leftEdgePx, float rightEdgePx,
                          float captionHeightPx, UINT dpi) {
    left_edge_px_  = leftEdgePx;
    right_edge_px_ = rightEdgePx;
    caption_h_px_  = captionHeightPx;
    dpi_           = dpi;
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

    // Reserve space for the "+" button on the right.
    const float available  = right_edge_px_ - left_edge_px_
                             - plusSzPx - plusGapPx;
    const float totalGaps  = gapPx * (n - 1);
    const float pillW      = std::clamp((available - totalGaps) / n,
                                        minWPx, maxWPx);

    // Centre the pill row in the available span (excluding "+").
    const float rowW   = pillW * n + totalGaps;
    // Align pills starting from left_edge_px_ with some centering.
    const float startX = left_edge_px_
                         + ((available - rowW) * 0.5f);
    const float pillY  = (caption_h_px_ - pillHPx) * 0.5f;

    for (int i = 0; i < n; ++i) {
        const float x0 = startX + i * (pillW + gapPx);
        pills_[i].rect   = {x0, pillY, x0 + pillW, pillY + pillHPx};
        pills_[i].active = (i == active_idx_);

        // Close button: top-right corner of the pill.
        pills_[i].closeRect = {
            pills_[i].rect.right  - closeMarPx - closeSzPx,
            pills_[i].rect.top    + closeMarPx,
            pills_[i].rect.right  - closeMarPx,
            pills_[i].rect.top    + closeMarPx + closeSzPx,
        };
    }

    // "+" button: right of the last pill.
    const float lastRight = pills_[n - 1].rect.right;
    const float plusY     = (caption_h_px_ - plusSzPx) * 0.5f;
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

    // Lazily create text format.
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
            fmt_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
            fmt_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
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
            const float labelAlpha = pill.active ? alpha * 0.90f :
                                     hov         ? alpha * 0.65f :
                                                   alpha * 0.45f;
            brush->SetColor(D2D1::ColorF(1.0f, 1.0f, 1.0f, labelAlpha));

            // Shrink label rect so it doesn't overlap the × button.
            D2D1_RECT_F labelRect = pill.rect;
            if (showClose)
                labelRect.right -= closeSzPx + theme::ToPx(kCloseMargin, dpi_);

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
            if (SUCCEEDED(dc->GetFactory()->CreatePathGeometry(xPath.GetAddressOf()))) {
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
                dc->GetFactory()->CreateStrokeStyle(sp, nullptr, 0,
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
        if (SUCCEEDED(dc->GetFactory()->CreatePathGeometry(plusPath.GetAddressOf()))) {
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
            dc->GetFactory()->CreateStrokeStyle(sp, nullptr, 0,
                                                ss.GetAddressOf());
            dc->DrawGeometry(plusPath.Get(), brush, 1.5f, ss.Get());
        }
    }
}

}  // namespace vrtx::ui
