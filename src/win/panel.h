#pragma once
#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>

#include <functional>

#include "core/light_state.h"
#include "core/panel_layout.h"

namespace rl {

class Panel {
public:
    using ChangeFn = std::function<void(const LightState&)>;
    bool create(HINSTANCE inst, ChangeFn onChange);
    void show(RECT iconRect, const LightState& s, bool mbOk, bool ramOk);
    void update(const LightState& s, bool mbOk, bool ramOk);  // repaints if visible
    void hide();
    bool visible() const { return hwnd_ && IsWindowVisible(hwnd_); }
    void destroy();

private:
    static LRESULT CALLBACK wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
    LRESULT handle(UINT msg, WPARAM wp, LPARAM lp);
    void paint();
    void click(float x, float y);
    void dragTo(float x, bool final);
    void pickCustom();
    void emit();
    float scale() const;
    bool ensureTarget();
    void releaseTarget();

    HWND hwnd_ = nullptr;
    ChangeFn onChange_;
    PanelLayout layout_ = panelLayout();
    LightState state_;
    bool mbOk_ = false, ramOk_ = false;
    bool dragging_ = false, inDialog_ = false;
    ApplyThrottle throttle_{40};
    ID2D1Factory* factory_ = nullptr;
    ID2D1HwndRenderTarget* rt_ = nullptr;
    IDWriteFactory* dw_ = nullptr;
    IDWriteTextFormat* textFmt_ = nullptr;
    IDWriteTextFormat* titleFmt_ = nullptr;
    IDWriteTextFormat* centerFmt_ = nullptr;
};

}  // namespace rl
