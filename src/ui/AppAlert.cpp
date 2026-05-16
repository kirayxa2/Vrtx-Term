#include "ui/AppAlert.h"

namespace mactw::ui {

namespace {

float EaseOutCubic(float t) {
    const float u = 1.0f - t;
    return 1.0f - u * u * u;
}

float Smoothstep(float lo, float hi, float x) {
    const float t = std::clamp((x - lo) / std::max(1e-6f, hi - lo), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

void EnsureFormat(IDWriteFactory* dwrite,
                  ComPtr<IDWriteTextFormat>& fmt,
                  float& builtAt,
                  float wantSize,
                  DWRITE_FONT_WEIGHT weight,
                  DWRITE_TEXT_ALIGNMENT align) {
    if (!dwrite) return;
    if (fmt && std::abs(builtAt - wantSize) <= 0.5f) return;

    fmt.Reset();
    dwrite->CreateTextFormat(
        L"Segoe UI", nullptr,
        weight,
        DWRITE_FONT_STYLE_NORMAL,
        DWRITE_FONT_STRETCH_NORMAL,
        wantSize, L"en-us",
        fmt.GetAddressOf());
    if (fmt) {
        fmt->SetWordWrapping(DWRITE_WORD_WRAPPING_WRAP);
        fmt->SetTextAlignment(align);
        fmt->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_NEAR);
    }
    builtAt = wantSize;
}

}  // namespace

// ---------------------------------------------------------------------------

void AppAlert::Show(std::wstring title,
                    std::wstring message,
                    std::wstring buttonText) {
    title_       = std::move(title);
    message_     = std::move(message);
    button_text_ = std::move(buttonText);
    if (button_text_.empty()) button_text_ = L"OK";

    open_           = true;
    closing_        = false;
    hover_button_   = false;
    pressed_button_ = false;
}

void AppAlert::UpdateLayout(D2D1_RECT_F squircleRect,
                            float captionHeightPx,
                            UINT dpi) {
    using namespace theme;

    padding_x_px_     = ToPx(kAlertPaddingX,        dpi);
    padding_y_px_     = ToPx(kAlertPaddingY,        dpi);
    title_size_px_    = ToPx(kAlertTitleSize,       dpi);
    msg_size_px_      = ToPx(kAlertMessageSize,     dpi);
    title_gap_px_     = ToPx(kAlertTitleGap,        dpi);
    msg_gap_px_       = ToPx(kAlertMessageGap,      dpi);
    btn_h_px_         = ToPx(kAlertButtonHeight,    dpi);
    btn_radius_px_    = ToPx(kAlertButtonRadius,    dpi);
    btn_text_size_px_ = ToPx(kAlertButtonTextSize,  dpi);
    panel_radius_px_  = ToPx(kAlertCornerRadius,    dpi);

    const float panelW = ToPx(kAlertWidth, dpi);

    // Estimate panel height from font sizes; this is fine for one-line
    // title and a short multi-line message (we wrap at ~3 lines worst
    // case, which is plenty for a settings/about dialog).
    const float titleLineH = title_size_px_ * 1.25f;
    const float msgLineH   = msg_size_px_   * 1.30f;
    const float msgBlockH  = msgLineH * 3.0f;   // up to 3 wrapped lines

    const float panelH = padding_y_px_ * 2.0f
                       + titleLineH
                       + title_gap_px_
                       + msgBlockH
                       + msg_gap_px_
                       + btn_h_px_;

    // Centre the panel inside the squircle's content area (everything
    // below the caption strip). This way the dialog never overlaps the
    // traffic-lights or the caption button - both stay clickable.
    const float contentTop  = squircleRect.top + captionHeightPx;
    const float contentLeft = squircleRect.left;
    const float contentW    = squircleRect.right  - squircleRect.left;
    const float contentH    = squircleRect.bottom - contentTop;

    const float left = contentLeft + (contentW - panelW) * 0.5f;
    const float top  = contentTop  + std::max(0.0f, (contentH - panelH) * 0.5f);

    panel_ = D2D1::RectF(left, top, left + panelW, top + panelH);

    // Primary button: full panel width minus padding, sitting at the
    // bottom of the panel.
    const float btnLeft  = panel_.left  + padding_x_px_;
    const float btnRight = panel_.right - padding_x_px_;
    const float btnBot   = panel_.bottom - padding_y_px_;
    const float btnTop   = btnBot - btn_h_px_;
    button_ = D2D1::RectF(btnLeft, btnTop, btnRight, btnBot);
}

bool AppAlert::HitTestPanel(int x, int y) const {
    if (!open_) return false;
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    return fx >= panel_.left && fx < panel_.right &&
           fy >= panel_.top  && fy < panel_.bottom;
}

bool AppAlert::HitTestButton(int x, int y) const {
    if (!open_) return false;
    const float fx = static_cast<float>(x);
    const float fy = static_cast<float>(y);
    return fx >= button_.left && fx < button_.right &&
           fy >= button_.top  && fy < button_.bottom;
}

void AppAlert::OnMouseMove(int x, int y) {
    if (!open_ || progress_ < 0.5f) {
        hover_button_ = false;
        return;
    }
    hover_button_ = HitTestButton(x, y);
}

void AppAlert::OnMouseLeave() {
    hover_button_ = false;
    pressed_button_ = false;
}

void AppAlert::OnLButtonDown(int x, int y) {
    pressed_button_ = HitTestButton(x, y);
}

bool AppAlert::OnLButtonUp(int x, int y) {
    const bool fire = pressed_button_ && HitTestButton(x, y);
    pressed_button_ = false;
    if (fire) {
        if (on_dismiss_) on_dismiss_();
        return true;
    }
    return false;
}

// ---------------------------------------------------------------------------

void AppAlert::Render(ID2D1DeviceContext* dc,
                      ID2D1SolidColorBrush* brush,
                      ID2D1Factory* /*factory*/,
                      IDWriteFactory* dwrite,
                      D2D1_RECT_F squircleRect,
                      float captionHeightPx) const {
    if (!open_ && progress_ <= 0.0f) return;

    const auto& pal = theme::ActivePalette();
    const float ease = EaseOutCubic(progress_);

    // ---- Scrim --------------------------------------------------------
    //
    // Dim the entire content area (caption strip excluded so traffic
    // lights stay visible). Apple's alerts darken everything below the
    // titlebar - same here.
    {
        const D2D1_RECT_F scrim{
            squircleRect.left,
            squircleRect.top + captionHeightPx,
            squircleRect.right,
            squircleRect.bottom,
        };
        const auto sc = pal.alertScrim;
        brush->SetColor(D2D1::ColorF(sc.r, sc.g, sc.b, sc.a * ease));
        dc->FillRectangle(scrim, brush);
    }

    // ---- Panel scale + fade -------------------------------------------
    //
    // Panel scales 0.95 -> 1.0 to suggest a gentle "pop" while fading in.
    const float scale = 0.95f + 0.05f * ease;
    const float opacity = ease;

    const float cx = (panel_.left + panel_.right)  * 0.5f;
    const float cy = (panel_.top  + panel_.bottom) * 0.5f;

    D2D1_MATRIX_3X2_F prev;
    dc->GetTransform(&prev);
    const D2D1_MATRIX_3X2_F xf = D2D1::Matrix3x2F::Scale(
        D2D1::Size(scale, scale), D2D1::Point2F(cx, cy));
    dc->SetTransform(xf * prev);

    // ---- Drop shadow under panel --------------------------------------
    {
        struct Halo { float dy; float spread; float alpha; };
        const Halo halos[] = {
            {12.0f, 12.0f, 0.22f * opacity},
            { 6.0f,  6.0f, 0.16f * opacity},
            { 2.0f,  2.0f, 0.10f * opacity},
        };
        for (const auto& h : halos) {
            const D2D1_ROUNDED_RECT g{
                D2D1::RectF(panel_.left   - h.spread,
                            panel_.top    - h.spread + h.dy,
                            panel_.right  + h.spread,
                            panel_.bottom + h.spread + h.dy),
                panel_radius_px_ + h.spread,
                panel_radius_px_ + h.spread,
            };
            brush->SetColor(D2D1::ColorF(0, 0, 0, h.alpha));
            dc->FillRoundedRectangle(g, brush);
        }
    }

    // ---- Panel fill ----------------------------------------------------
    {
        const D2D1_ROUNDED_RECT panel{panel_, panel_radius_px_, panel_radius_px_};
        const auto base = pal.alertPanelFill;
        brush->SetColor(D2D1::ColorF(base.r, base.g, base.b, base.a * opacity));
        dc->FillRoundedRectangle(panel, brush);
    }

    // ---- Title + message + button -------------------------------------
    EnsureFormat(dwrite, title_fmt_, built_at_title_,
                 title_size_px_, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                 DWRITE_TEXT_ALIGNMENT_CENTER);
    EnsureFormat(dwrite, msg_fmt_, built_at_msg_,
                 msg_size_px_, DWRITE_FONT_WEIGHT_NORMAL,
                 DWRITE_TEXT_ALIGNMENT_CENTER);
    EnsureFormat(dwrite, btn_fmt_, built_at_btn_,
                 btn_text_size_px_, DWRITE_FONT_WEIGHT_SEMI_BOLD,
                 DWRITE_TEXT_ALIGNMENT_CENTER);
    if (btn_fmt_) btn_fmt_->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);

    const float textFade = Smoothstep(0.3f, 1.0f, progress_);

    if (textFade > 0.0f) {
        // Title.
        if (title_fmt_ && !title_.empty()) {
            const D2D1_RECT_F titleRect{
                panel_.left  + padding_x_px_,
                panel_.top   + padding_y_px_,
                panel_.right - padding_x_px_,
                panel_.top   + padding_y_px_ + title_size_px_ * 1.4f,
            };
            const auto t = pal.alertTitle;
            brush->SetColor(D2D1::ColorF(t.r, t.g, t.b, t.a * textFade));
            dc->DrawTextW(title_.c_str(),
                          static_cast<UINT32>(title_.size()),
                          title_fmt_.Get(), titleRect, brush);
        }

        // Message.
        if (msg_fmt_ && !message_.empty()) {
            const float msgTop = panel_.top + padding_y_px_
                               + title_size_px_ * 1.4f + title_gap_px_;
            const D2D1_RECT_F msgRect{
                panel_.left  + padding_x_px_,
                msgTop,
                panel_.right - padding_x_px_,
                button_.top  - msg_gap_px_,
            };
            const auto m = pal.alertMessage;
            brush->SetColor(D2D1::ColorF(m.r, m.g, m.b, m.a * textFade));
            dc->DrawTextW(message_.c_str(),
                          static_cast<UINT32>(message_.size()),
                          msg_fmt_.Get(), msgRect, brush);
        }

        // Primary button.
        {
            const D2D1_ROUNDED_RECT btn{button_,
                                        btn_radius_px_, btn_radius_px_};
            const auto fill = pal.alertButtonFill;
            brush->SetColor(D2D1::ColorF(fill.r, fill.g, fill.b,
                                          fill.a * opacity));
            dc->FillRoundedRectangle(btn, brush);

            if (hover_button_ || pressed_button_) {
                const auto h = pal.alertButtonHover;
                const float strength = pressed_button_ ? 1.6f : 1.0f;
                brush->SetColor(D2D1::ColorF(
                    h.r, h.g, h.b,
                    std::min(1.0f, h.a * strength) * opacity));
                dc->FillRoundedRectangle(btn, brush);
            }

            if (btn_fmt_ && !button_text_.empty()) {
                const auto bt = pal.alertButtonText;
                brush->SetColor(D2D1::ColorF(bt.r, bt.g, bt.b,
                                              bt.a * textFade));
                dc->DrawTextW(button_text_.c_str(),
                              static_cast<UINT32>(button_text_.size()),
                              btn_fmt_.Get(), button_, brush);
            }
        }
    }

    dc->SetTransform(prev);
}

}  // namespace mactw::ui
