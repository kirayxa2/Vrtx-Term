// Tab bar drawn inside the caption strip, between the traffic lights
// and the caption (more) button.
//
// macOS-style tabs: pill-shaped, centred in the caption strip height.
// Active tab: solid fill with a subtle glow.
// Inactive tabs: ghost (outline only), fade in on hover.
// "+" button: small circular button to the right of the last tab.
// Close (x): shown on hover of active tab.
//
// Coordinates are squircle-local physical pixels, same convention as
// TrafficLights / CaptionButton.

#pragma once

#include "pch.h"

namespace vrtx::ui {

struct Tab {
    std::wstring title;    // displayed label
    bool         active;   // currently selected
};

class TabBar {
public:
    // Callbacks set by the window.
    using NewTabHandler   = std::function<void()>;
    using CloseTabHandler = std::function<void(int index)>;
    using SwitchHandler   = std::function<void(int index)>;

    void SetOnNewTab  (NewTabHandler   h) { on_new_tab_   = std::move(h); }
    void SetOnClose   (CloseTabHandler h) { on_close_     = std::move(h); }
    void SetOnSwitch  (SwitchHandler   h) { on_switch_    = std::move(h); }

    // Rebuild layout for a tab strip occupying the rect `stripRect`
    // (squircle-local physical pixels). The pills + "+" group is
    // centred horizontally inside the strip.
    void UpdateLayout(D2D1_RECT_F stripRect, UINT dpi);

    // Strip rect (for the renderer to fill the background).
    D2D1_RECT_F StripRect() const { return strip_rect_; }

    // Replace the tab list. Index `activeIdx` is the selected tab.
    void SetTabs(std::vector<Tab> tabs, int activeIdx);

    // Interaction.
    void OnMouseMove(float x, float y);
    void OnMouseLeave();
    // Handles click — fires callbacks internally. Returns true if consumed.
    bool OnLButtonUp(float x, float y);

    // Returns true if (x,y) is inside any tab pill or the "+" button.
    bool HitTest(float x, float y) const;

    void Render(ID2D1DeviceContext* dc,
                ID2D1SolidColorBrush* brush,
                IDWriteFactory*       dwrite,
                bool                  windowActive) const;

    // Number of tabs currently managed.
    int Count() const { return static_cast<int>(tabs_.size()); }
    int ActiveIdx() const { return active_idx_; }

private:
    struct Pill {
        D2D1_RECT_F rect{};       // squircle-local bounding rect of pill
        D2D1_RECT_F closeRect{};  // small "x" hit area (top-right of pill)
        bool        active{false};
        bool        hoverClose{false};
    };

    std::vector<Tab>  tabs_;
    std::vector<Pill> pills_;
    int               active_idx_{0};
    int               hover_idx_{-1};   // -1 = none, tab pills
    bool              hover_plus_{false};

    // Layout data saved for RebuildPills after SetTabs.
    D2D1_RECT_F strip_rect_{};
    UINT  dpi_{96};

    // "+" button rect.
    D2D1_RECT_F plus_rect_{};

    ComPtr<IDWriteTextFormat> fmt_;

    NewTabHandler   on_new_tab_;
    CloseTabHandler on_close_;
    SwitchHandler   on_switch_;

    void RebuildPills();
};

}  // namespace vrtx::ui
