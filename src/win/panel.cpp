#include "win/panel.h"

#include <commdlg.h>
#include <windowsx.h>

#include <cmath>

#include "core/status.h"
#include "win/messages.h"
#include "win/text.h"

namespace rl {

namespace {
const wchar_t* kPanelClass = L"RigLightPanel";

struct Theme {
    D2D1_COLOR_F bg, text, subtle, control, border, accent;
};

D2D1_COLOR_F hex(uint32_t rgb, float a = 1.0f) {
    return D2D1::ColorF(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, a);
}

Theme currentTheme() {
    DWORD light = 0, size = sizeof light;
    RegGetValueW(HKEY_CURRENT_USER, L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
                 L"SystemUsesLightTheme", RRF_RT_REG_DWORD, nullptr, &light, &size);
    if (light) return {hex(0xF3F3F3), hex(0x1A1A1A), hex(0x5F5F5F), hex(0xFFFFFF), hex(0xD0D0D0), hex(0x005FB8)};
    return {hex(0x202020), hex(0xFFFFFF), hex(0x9A9A9A), hex(0x2D2D2D), hex(0x3A3A3A), hex(0x4CC2FF)};
}

D2D1_RECT_F toD2D(const RectF& r) { return D2D1::RectF(r.x, r.y, r.x + r.w, r.y + r.h); }

template <class T>
void release(T*& p) {
    if (p) p->Release();
    p = nullptr;
}
}  // namespace

bool Panel::create(HINSTANCE inst, ChangeFn onChange) {
    onChange_ = std::move(onChange);
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &factory_))) return false;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                                   reinterpret_cast<IUnknown**>(&dw_))))
        return false;
    dw_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"", &textFmt_);
    dw_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_SEMI_BOLD, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, 15.0f, L"", &titleFmt_);
    dw_->CreateTextFormat(L"Segoe UI", nullptr, DWRITE_FONT_WEIGHT_NORMAL, DWRITE_FONT_STYLE_NORMAL,
                          DWRITE_FONT_STRETCH_NORMAL, 12.0f, L"", &centerFmt_);
    for (IDWriteTextFormat* f : {textFmt_, titleFmt_, centerFmt_})
        f->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
    centerFmt_->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof wc;
    wc.style = CS_DROPSHADOW;
    wc.lpfnWndProc = &Panel::wndProc;
    wc.hInstance = inst;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = kPanelClass;
    RegisterClassExW(&wc);
    hwnd_ = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_TOPMOST, kPanelClass, L"RigLight", WS_POPUP, 0, 0, 10, 10,
                            nullptr, nullptr, inst, this);
    return hwnd_ != nullptr;
}

void Panel::destroy() {
    releaseTarget();
    release(textFmt_);
    release(titleFmt_);
    release(centerFmt_);
    release(dw_);
    release(factory_);
    if (hwnd_) DestroyWindow(hwnd_);
    hwnd_ = nullptr;
}

float Panel::scale() const { return hwnd_ ? GetDpiForWindow(hwnd_) / 96.0f : 1.0f; }

void Panel::show(RECT iconRect, const LightState& s, bool mbOk, bool ramOk) {
    state_ = s;
    mbOk_ = mbOk;
    ramOk_ = ramOk;
    HMONITOR mon = MonitorFromRect(&iconRect, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof mi;
    GetMonitorInfoW(mon, &mi);
    // Size for the monitor that holds the icon (the window may currently sit on another one).
    UINT mdpiX = 96, mdpiY = 96;
    using GetDpiForMonitorFn = HRESULT(WINAPI*)(HMONITOR, int, UINT*, UINT*);
    if (HMODULE shcore = LoadLibraryW(L"shcore.dll")) {
        if (auto fn = reinterpret_cast<GetDpiForMonitorFn>(
                reinterpret_cast<void*>(GetProcAddress(shcore, "GetDpiForMonitor"))))
            fn(mon, 0 /* MDT_EFFECTIVE_DPI */, &mdpiX, &mdpiY);
        FreeLibrary(shcore);
    }
    const float sc = mdpiX / 96.0f;
    const int w = (int)std::lround(layout_.width * sc), h = (int)std::lround(layout_.height * sc);
    const RectI r = placePanel(RectI{iconRect.left, iconRect.top, iconRect.right, iconRect.bottom},
                               RectI{mi.rcWork.left, mi.rcWork.top, mi.rcWork.right, mi.rcWork.bottom}, w, h);
    SetWindowPos(hwnd_, HWND_TOPMOST, r.left, r.top, w, h, SWP_SHOWWINDOW);
    SetForegroundWindow(hwnd_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Panel::update(const LightState& s, bool mbOk, bool ramOk) {
    if (dragging_) return;  // the drag owns the slider value until mouse-up
    state_ = s;
    mbOk_ = mbOk;
    ramOk_ = ramOk;
    if (visible()) InvalidateRect(hwnd_, nullptr, FALSE);
}

void Panel::hide() {
    if (hwnd_) ShowWindow(hwnd_, SW_HIDE);
    dragging_ = false;
}

bool Panel::ensureTarget() {
    if (rt_) return true;
    RECT rc;
    GetClientRect(hwnd_, &rc);
    const float dpi = (float)GetDpiForWindow(hwnd_);
    const auto props = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                                                    D2D1::PixelFormat(), dpi, dpi);
    return SUCCEEDED(factory_->CreateHwndRenderTarget(
        props, D2D1::HwndRenderTargetProperties(hwnd_, D2D1::SizeU(rc.right, rc.bottom)), &rt_));
}

void Panel::releaseTarget() { release(rt_); }

void Panel::paint() {
    if (!ensureTarget()) return;
    const Theme t = currentTheme();
    const bool on = state_.on;
    const bool rainbow = state_.effect == Effect::Rainbow;
    ID2D1SolidColorBrush* b = nullptr;
    rt_->CreateSolidColorBrush(t.text, &b);
    auto fill = [&](D2D1_COLOR_F c) { b->SetColor(c); return b; };
    auto text = [&](const std::wstring& s, const RectF& r, IDWriteTextFormat* f, D2D1_COLOR_F c) {
        rt_->DrawTextW(s.c_str(), (UINT32)s.size(), f, toD2D(r), fill(c));
    };

    rt_->BeginDraw();
    rt_->Clear(t.bg);
    text(L"RigLight", layout_.title, titleFmt_, t.text);

    // Power pill
    const RectF& p = layout_.power;
    const auto pill = D2D1::RoundedRect(toD2D(p), p.h / 2, p.h / 2);
    if (on) {
        rt_->FillRoundedRectangle(pill, fill(t.accent));
    } else {
        rt_->FillRoundedRectangle(pill, fill(t.control));
        rt_->DrawRoundedRectangle(pill, fill(t.border), 1.0f);
    }
    const float kr = p.h / 2 - 4;
    const float kx = on ? p.x + p.w - p.h / 2 : p.x + p.h / 2;
    rt_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(kx, p.y + p.h / 2), kr, kr), fill(on ? hex(0xFFFFFF) : t.subtle));

    const float dim = on ? 1.0f : 0.45f;
    text(L"Color", layout_.colorLabel, textFmt_, t.subtle);
    const int sel = rainbow ? -2 : swatchIndexOf(state_.color);
    for (int i = 0; i < 8; ++i) {
        const RectF& r = layout_.swatches[i];
        const Rgb c = kSwatches[i];
        const auto e = D2D1::Ellipse(D2D1::Point2F(r.x + r.w / 2, r.y + r.h / 2), r.w / 2 - 2, r.h / 2 - 2);
        rt_->FillEllipse(e, fill(D2D1::ColorF(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, (rainbow ? 0.35f : 1.0f) * dim)));
        rt_->DrawEllipse(e, fill(t.border), 1.0f);
        if (sel == i)
            rt_->DrawEllipse(D2D1::Ellipse(e.point, r.w / 2 + 1, r.h / 2 + 1), fill(t.accent), 2.0f);
    }
    {
        const RectF& r = layout_.custom;
        const auto e = D2D1::Ellipse(D2D1::Point2F(r.x + r.w / 2, r.y + r.h / 2), r.w / 2 - 2, r.h / 2 - 2);
        if (sel == -1) {
            const Rgb c = state_.color;
            rt_->FillEllipse(e, fill(D2D1::ColorF(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, dim)));
            rt_->DrawEllipse(D2D1::Ellipse(e.point, r.w / 2 + 1, r.h / 2 + 1), fill(t.accent), 2.0f);
        } else {
            rt_->FillEllipse(e, fill(t.control));
            rt_->DrawEllipse(e, fill(t.border), 1.0f);
            text(L"+", r, centerFmt_, rainbow ? t.border : t.text);
        }
    }

    text(L"Effect", layout_.effectLabel, textFmt_, t.subtle);
    const wchar_t* names[3] = {L"Static", L"Breathing", L"Rainbow"};
    const RectF all{layout_.effects[0].x, layout_.effects[0].y,
                    layout_.effects[2].x + layout_.effects[2].w - layout_.effects[0].x, layout_.effects[0].h};
    rt_->FillRoundedRectangle(D2D1::RoundedRect(toD2D(all), 6, 6), fill(t.control));
    for (int i = 0; i < 3; ++i) {
        const RectF& r = layout_.effects[i];
        const bool active = (int)state_.effect == i;
        if (active)
            rt_->FillRoundedRectangle(
                D2D1::RoundedRect(D2D1::RectF(r.x + 2, r.y + 2, r.x + r.w - 2, r.y + r.h - 2), 5, 5),
                fill(D2D1::ColorF(t.accent.r, t.accent.g, t.accent.b, dim)));
        text(names[i], r, centerFmt_, active ? hex(0xFFFFFF) : t.text);
    }

    const int pct = clampBrightness(state_.brightness);
    text(L"Brightness   " + std::to_wstring(pct) + L"%", layout_.brightnessLabel, textFmt_, t.subtle);
    const RectF& tr = layout_.track;
    const float kxs = sliderKnobX(layout_, pct);
    rt_->FillRectangle(D2D1::RectF(tr.x, tr.y - 1, tr.x + tr.w, tr.y + tr.h + 1), fill(t.border));
    rt_->FillRectangle(D2D1::RectF(tr.x, tr.y - 1, kxs, tr.y + tr.h + 1),
                       fill(D2D1::ColorF(t.accent.r, t.accent.g, t.accent.b, dim)));
    const auto knob = D2D1::Ellipse(D2D1::Point2F(kxs, tr.y + tr.h / 2), 8, 8);
    rt_->FillEllipse(knob, fill(hex(0xFFFFFF)));
    rt_->DrawEllipse(knob, fill(t.border), 1.0f);

    text(widen(footerText(mbOk_, ramOk_)), layout_.footer, textFmt_, t.subtle);

    if (rt_->EndDraw() == D2DERR_RECREATE_TARGET) releaseTarget();
    b->Release();
}

void Panel::emit() {
    if (onChange_) onChange_(state_);
}

void Panel::click(float x, float y) {
    const PanelHitResult h = panelHitTest(layout_, x, y);
    switch (h.kind) {
        case PanelHit::Power:
            state_.on = !state_.on;
            emit();
            break;
        case PanelHit::Swatch:
            if (state_.effect == Effect::Rainbow) return;  // colors are disabled for rainbow
            state_.color = kSwatches[h.index];
            state_.on = true;
            emit();
            break;
        case PanelHit::Custom:
            if (state_.effect == Effect::Rainbow) return;
            pickCustom();
            return;
        case PanelHit::Effect:
            state_.effect = (Effect)h.index;
            state_.on = true;
            emit();
            break;
        case PanelHit::Slider:
            dragging_ = true;
            SetCapture(hwnd_);
            dragTo(x, false);
            return;
        case PanelHit::None:
            return;
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Panel::dragTo(float x, bool final) {
    state_.brightness = sliderValueAt(layout_, x);
    state_.on = true;
    const uint64_t now = GetTickCount64();
    if (final) {
        emit();  // the last value always goes out
    } else if (throttle_.request(now)) {
        emit();
    } else {
        SetTimer(hwnd_, kThrottleTimerId, 15, nullptr);
    }
    InvalidateRect(hwnd_, nullptr, FALSE);
}

void Panel::pickCustom() {
    static COLORREF custom[16] = {};
    CHOOSECOLORW cc{};
    cc.lStructSize = sizeof cc;
    cc.hwndOwner = hwnd_;
    cc.rgbResult = RGB(state_.color.r, state_.color.g, state_.color.b);
    cc.lpCustColors = custom;
    cc.Flags = CC_FULLOPEN | CC_RGBINIT;
    inDialog_ = true;  // keep the panel open while the dialog has focus
    const BOOL ok = ChooseColorW(&cc);
    inDialog_ = false;
    if (ok) {
        state_.color = Rgb{GetRValue(cc.rgbResult), GetGValue(cc.rgbResult), GetBValue(cc.rgbResult)};
        state_.on = true;
        emit();
    }
    SetForegroundWindow(hwnd_);
    InvalidateRect(hwnd_, nullptr, FALSE);
}

LRESULT CALLBACK Panel::wndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, (LONG_PTR)cs->lpCreateParams);
        reinterpret_cast<Panel*>(cs->lpCreateParams)->hwnd_ = hwnd;
    }
    auto* self = reinterpret_cast<Panel*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    return self ? self->handle(msg, wp, lp) : DefWindowProcW(hwnd, msg, wp, lp);
}

LRESULT Panel::handle(UINT msg, WPARAM wp, LPARAM lp) {
    const float sc = scale();
    switch (msg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            BeginPaint(hwnd_, &ps);
            paint();
            EndPaint(hwnd_, &ps);
            return 0;
        }
        case WM_SIZE:
            if (rt_) rt_->Resize(D2D1::SizeU(LOWORD(lp), HIWORD(lp)));
            return 0;
        case WM_DPICHANGED:
            releaseTarget();
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(wp) == WA_INACTIVE && !inDialog_) hide();
            return 0;
        case WM_KEYDOWN:
            if (wp == VK_ESCAPE) hide();
            return 0;
        case WM_LBUTTONDOWN:
            click(GET_X_LPARAM(lp) / sc, GET_Y_LPARAM(lp) / sc);
            return 0;
        case WM_MOUSEMOVE:
            if (dragging_) dragTo(GET_X_LPARAM(lp) / sc, false);
            return 0;
        case WM_LBUTTONUP:
            if (dragging_) {
                dragging_ = false;
                ReleaseCapture();
                KillTimer(hwnd_, kThrottleTimerId);
                dragTo(GET_X_LPARAM(lp) / sc, true);
            }
            return 0;
        case WM_TIMER:
            if (wp == kThrottleTimerId && throttle_.flushDue(GetTickCount64())) {
                KillTimer(hwnd_, kThrottleTimerId);
                emit();
            }
            return 0;
    }
    return DefWindowProcW(hwnd_, msg, wp, lp);
}

}  // namespace rl
