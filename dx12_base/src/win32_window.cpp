// win32_window.cpp
#include "win32_window.h"

namespace dx12base {

namespace {

constexpr wchar_t kWindowClass[] = L"VkQ3Dx12BaseWindow";

// Pointer to the Win32Window instance, hung off the HWND via GWLP_USERDATA.
// A static is not an option because the sample is not limited to one window.
Win32Window* g_activeWindow = nullptr;

}  // namespace

bool Win32Window::Create(const wchar_t* title, int width, int height, bool fullscreen) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = kWindowClass;

    if (!RegisterClassExW(&windowClass)) {
        return false;
    }

    // Opt into per-monitor DPI so the client area is what we asked for.
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    RECT rect{0, 0, width, height};
    AdjustWindowRectEx(&rect, WS_OVERLAPPEDWINDOW, FALSE, 0);

    const int windowWidth = rect.right - rect.left;
    const int windowHeight = rect.bottom - rect.top;

    hwnd_ = CreateWindowExW(
        0, kWindowClass, title, WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        windowWidth, windowHeight,
        nullptr, nullptr, instance_, this);

    if (!hwnd_) {
        return false;
    }

    ShowWindow(hwnd_, SW_SHOW);
    UpdateWindow(hwnd_);

    width_ = width;
    height_ = height;
    fullscreen_ = false;

    if (fullscreen) {
        SetFullscreen(true);
    }

    return true;
}

void Win32Window::Destroy() {
    if (!hwnd_) {
        return;
    }
    DestroyWindow(hwnd_);
    hwnd_ = nullptr;
    UnregisterClassW(kWindowClass, instance_);
}

bool Win32Window::PumpMessages() {
    MSG message{};
    while (PeekMessage(&message, nullptr, 0, 0, PM_REMOVE)) {
        TranslateMessage(&message);
        DispatchMessage(&message);

        if (message.message == WM_QUIT) {
            quit_ = true;
        }
    }
    return !quit_;
}

void Win32Window::GetClientSize(int& width, int& height) const {
    if (!hwnd_) {
        width = width_;
        height = height_;
        return;
    }

    RECT rect{};
    GetClientRect(hwnd_, &rect);
    width = rect.right - rect.left;
    height = rect.bottom - rect.top;
}

void Win32Window::SetFullscreen(bool enabled) {
    if (!hwnd_ || enabled == fullscreen_) {
        return;
    }

    if (enabled) {
        // Remember where we were so we can restore it.
        RECT rect{};
        GetWindowRect(hwnd_, &rect);
        windowedX_ = rect.left;
        windowedY_ = rect.top;
        windowedWidth_ = rect.right - rect.left;
        windowedHeight_ = rect.bottom - rect.top;

        MONITORINFO monitorInfo{sizeof(monitorInfo)};
        if (GetMonitorInfo(MonitorFromWindow(hwnd_, MONITOR_DEFAULTTOPRIMARY), &monitorInfo)) {
            SetWindowLongW(hwnd_, GWL_STYLE, WS_POPUP);
            SetWindowPos(hwnd_, HWND_TOP,
                         monitorInfo.rcMonitor.left, monitorInfo.rcMonitor.top,
                         monitorInfo.rcMonitor.right - monitorInfo.rcMonitor.left,
                         monitorInfo.rcMonitor.bottom - monitorInfo.rcMonitor.top,
                         SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
        }
    } else {
        SetWindowLongW(hwnd_, GWL_STYLE, WS_OVERLAPPEDWINDOW);
        SetWindowPos(hwnd_, nullptr, windowedX_, windowedY_,
                     windowedWidth_, windowedHeight_,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }

    fullscreen_ = enabled;
    ShowWindow(hwnd_, SW_SHOW);
}

LRESULT CALLBACK Win32Window::WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
    if (message == WM_NCCREATE) {
        // The CREATESTRUCT carries our `this` pointer.
        auto* createStruct = reinterpret_cast<CREATESTRUCTW*>(lParam);
        g_activeWindow = static_cast<Win32Window*>(createStruct->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(g_activeWindow));
    }

    Win32Window* window = reinterpret_cast<Win32Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (window) {
        return window->HandleMessage(message, wParam, lParam);
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

LRESULT Win32Window::HandleMessage(UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_DESTROY:
            quit_ = true;
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            if (wParam == SIZE_MINIMIZED) {
                minimized_ = true;
            } else if (wParam == SIZE_RESTORED || wParam == SIZE_MAXIMIZED) {
                minimized_ = false;
                resized_ = true;
            }
            return 0;

        case WM_GETMINMAXINFO: {
            auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
            info->ptMinTrackSize.x = 800;
            info->ptMinTrackSize.y = 600;
            return 0;
        }

        case WM_SYSCOMMAND:
            // Swallow the Alt+F4 / Alt+Tab menu so fullscreen behaves.
            if ((wParam & 0xFFF0) == SC_KEYMENU) {
                return 0;
            }
            break;
    }

    return DefWindowProcW(hwnd_, message, wParam, lParam);
}

}  // namespace dx12base