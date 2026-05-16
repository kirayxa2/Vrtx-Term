// In-window alert dialog rendered through D2D - replaces MessageBoxW.
//
// macOS Tahoe shows alerts as a Liquid Glass "sheet" that fades in over
// the host window with a dim scrim covering the content. We do the same
// here so the alerts feel like a continuation of the chrome we already
// drew (same fonts, same shadows, same easing) rather than a foreign
// Win32 dialog with system-rendered decorations.
//
// API shape mirrors CaptionMenu so the window can host both with the
// same plumbing:
//
//   * `Show(title, message, primary)` arms the alert and starts the
//     opening animation.
//   * `IsOpen()` is true while progress > 0 (i.e. animating in OR out OR
//     fully open).
//   * `Render()` draws scrim + panel + button.
//   * `OnLButtonUp()` returns true if the dialog dismissed itself; the
//     window then animates the close.
//   * The scrim swallows clicks anywhere outside the panel; clicks
//     inside the panel only trigger when the user releases on the
//     primary button.
//
// Today we only support a single primary "OK" button. That's enough to
// replace every MessageBoxW we currently call. Adding "Cancel" or
// destructive secondary buttons later is a layout-only change.

#pragma once

#include "pch.h"
#include "theme/TahoeTheme.h"

namespace mactw::ui {

class AppAlert {
public:
    using DismissHandler = std::function<void()>;

    // Mark the alert as visible. The window starts the animation timer
    // separately - we just stash the strings and reset hover/press state
    // so the panel renders correctly on the next paint.
    void Show(std::wstring title,
              std::wstring message,
              std::wstring buttonText);

    // Begin closing. After SetProgress() reaches 0 the window calls
    // SetOpen(false). The open_ flag stays true through the fade-out so
    // hit-testing keeps blocking clicks from leaking to the terminal.
    void RequestClose() { closing_ = true; }

    // Layout has to be recomputed against the squircle's content area
    // (caption strip excluded). We center the panel in that rect.
    void UpdateLayout(D2D1_RECT_F squircleRect, float captionHeightPx,
                      UINT dpi);

    bool IsOpen()    const { return open_; }
    bool IsClosing() const { return closing_; }
    bool IsAnimating() const { return progress_ > 0.0f && progress_ < 1.0f; }

    void SetOpen(bool open) {
        open_ = open;
        if (!open) {
            closing_ = false;
            hover_button_ = false;
            pressed_button_ = false;
        }
    }
    void SetProgress(float p) { progress_ = std::clamp(p, 0.0f, 1.0f); }
    float Progress() const { return progress_; }

    bool HitTestPanel(int x, int y) const;
    bool HitTestButton(int x, int y) const;

    void OnMouseMove(int x, int y);
    void OnMouseLeave();
    void OnLButtonDown(int x, int y);
    // Returns true if the user committed the alert (clicked the primary
    // button). The window then starts the close animation.
    bool OnLButtonUp(int x, int y);

    // Optional callback fired when the user commits.
    void SetOnDismiss(DismissHandler h) { on_dismiss_ = std::move(h); }

    void Render(ID2D1DeviceContext* dc,
                ID2D1SolidColorBrush* brush,
                ID2D1Factory* factory,
                IDWriteFactory* dwrite,
                D2D1_RECT_F squircleRect,
                float captionHeightPx) const;

private:
    // Layout cache (squircle-local px).
    D2D1_RECT_F panel_  {};
    D2D1_RECT_F button_ {};
    float       padding_x_px_  {0};
    float       padding_y_px_  {0};
    float       title_size_px_ {0};
    float       msg_size_px_   {0};
    float       title_gap_px_  {0};
    float       msg_gap_px_    {0};
    float       btn_h_px_      {0};
    float       btn_radius_px_ {0};
    float       btn_text_size_px_{0};
    float       panel_radius_px_{0};

    bool   open_     {false};
    bool   closing_  {false};
    float  progress_ {0.0f};

    bool   hover_button_  {false};
    bool   pressed_button_{false};

    std::wstring title_;
    std::wstring message_;
    std::wstring button_text_;

    DismissHandler on_dismiss_;

    // DWrite format cache. Same logic as CaptionMenu - only rebuild on
    // DPI/size changes to keep the fade-in smooth.
    mutable ComPtr<IDWriteTextFormat> title_fmt_;
    mutable ComPtr<IDWriteTextFormat> msg_fmt_;
    mutable ComPtr<IDWriteTextFormat> btn_fmt_;
    mutable float built_at_title_{0};
    mutable float built_at_msg_  {0};
    mutable float built_at_btn_  {0};
};

}  // namespace mactw::ui
