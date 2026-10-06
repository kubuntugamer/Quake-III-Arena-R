// win32_window.h - Win32 window creation, message pump and input state
#pragma once

#include <Windows.h>

#include <string>

namespace dx12base {

// Thin wrapper over a plain Win32 window. Deliberately not integrated with
// the Quake input system so it can be lifted out as-is; see README.
class Win32Window {
public:
    bool Create(const wchar_t* title, int width, int height, bool fullscreen);
    void Destroy();

    HWND Handle() const { return hwnd_; }
    int Width() const { return width_; }
    int Height() const { return height_; }
    bool IsFullscreen() const { return fullscreen_; }
    void SetFullscreen(bool enabled);

    // Pumps the message queue. Returns false once the user closes the window.
    bool PumpMessages();

    bool WasResized() const { return resized_; }
    void ClearResized() { resized_ = false; }

    // Reads the client-area size, accounting for DPI.
    void GetClientSize(int& width, int& height) const;

private:
    static LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT HandleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    HWND hwnd_ = nullptr;
    HINSTANCE instance_ = GetModuleHandle(nullptr);
    int width_ = 1280;
    int height_ = 720;
    bool fullscreen_ = false;
    bool resized_ = false;
    bool quit_ = false;
    bool minimized_ = false;

    // Remembers the windowed state so toggling fullscreen can restore it.
    int windowedWidth_ = 1280;
    int windowedHeight_ = 720;
    int windowedX_ = 0;
    int windowedY_ = 0;
};

}  // namespace dx12base