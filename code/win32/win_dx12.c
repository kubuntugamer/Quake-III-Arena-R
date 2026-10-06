/*
===========================================================================
Win32 Platform Layer for DX12
===========================================================================
*/
#include "../qcommon/q_shared.h"
#include "../renderer/dx12/dx12_local.h"
#include "../renderer/dx12/dx12_main.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

// Window state
static HWND g_hwnd = NULL;
static HINSTANCE g_hInstance = NULL;
static int g_windowWidth = 1920;
static int g_windowHeight = 1080;
static BOOL g_fullscreen = FALSE;
static BOOL g_vsync = TRUE;
static BOOL g_running = TRUE;

// Forward declarations
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void Win32_InitWindow(int width, int height, BOOL fullscreen);
void Win32_ShutdownWindow(void);
void Win32_ProcessEvents(void);
void Win32_SetWindowTitle(const char* title);
void Win32_ParseCommandLine(LPWSTR cmdLine);

// Main entry point
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    g_hInstance = hInstance;
    
    // Parse command line
    Win32_ParseCommandLine(GetCommandLineW());
    
    // Initialize window
    Win32_InitWindow(g_windowWidth, g_windowHeight, g_fullscreen);
    
    // Initialize DX12
    if (!DX12_InitDevice(g_hwnd)) {
        MessageBox(NULL, L"Failed to initialize DX12", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    if (!DX12_CreateSwapchain(g_hwnd, g_windowWidth, g_windowHeight, g_fullscreen, g_vsync)) {
        MessageBox(NULL, L"Failed to create swapchain", L"Error", MB_OK | MB_ICONERROR);
        DX12_ShutdownDevice();
        return 1;
    }
    
    // Initialize renderer
    if (!DX12_InitRenderer()) {
        MessageBox(NULL, L"Failed to initialize renderer", L"Error", MB_OK | MB_ICONERROR);
        DX12_ShutdownDevice();
        return 1;
    }
    
    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);
    
    // Main loop
    MSG msg = {0};
    while (g_running) {
        // Process messages
        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
            if (msg.message == WM_QUIT) {
                g_running = FALSE;
                break;
            }
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        
        if (!g_running) break;
        
        // Render frame
        DX12_RenderFrame();
    }
    
    // Cleanup
    DX12_ShutdownRenderer();
    DX12_ShutdownDevice();
    Win32_ShutdownWindow();
    
    return 0;
}

void Win32_ParseCommandLine(LPWSTR cmdLine) {
    // Simple command line parsing for -width, -height, -fullscreen, -novsync
    // This is a simplified version - a full implementation would use CommandLineToArgvW
    char cmdLineA[4096];
    WideCharToMultiByte(CP_UTF8, 0, cmdLine, -1, cmdLineA, sizeof(cmdLineA), NULL, NULL);
    
    char* token = strtok(cmdLineA, " ");
    while (token) {
        if (strcmp(token, "-width") == 0) {
            token = strtok(NULL, " ");
            if (token) g_windowWidth = atoi(token);
        } else if (strcmp(token, "-height") == 0) {
            token = strtok(NULL, " ");
            if (token) g_windowHeight = atoi(token);
        } else if (strcmp(token, "-fullscreen") == 0) {
            g_fullscreen = TRUE;
        } else if (strcmp(token, "-novsync") == 0) {
            g_vsync = FALSE;
        }
        token = strtok(NULL, " ");
    }
}

void Win32_InitWindow(int width, int height, BOOL fullscreen) {
    WNDCLASSEXW wc = {0};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = g_hInstance;
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.lpszClassName = L"VkQ3NG_DX12";
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = LoadIcon(NULL, IDI_APPLICATION);
    
    RegisterClassExW(&wc);
    
    DWORD style = WS_OVERLAPPEDWINDOW;
    DWORD exStyle = WS_EX_APPWINDOW;
    
    if (fullscreen) {
        style = WS_POPUP | WS_VISIBLE;
        exStyle |= WS_EX_TOPMOST;
    }
    
    RECT rect = {0, 0, width, height};
    AdjustWindowRectEx(&rect, style, FALSE, exStyle);
    
    int windowWidth = rect.right - rect.left;
    int windowHeight = rect.bottom - rect.top;
    
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);
    int x = (screenWidth - windowWidth) / 2;
    int y = (screenHeight - windowHeight) / 2;
    
    g_hwnd = CreateWindowExW(
        exStyle,
        L"VkQ3NG_DX12",
        L"Vk Quake III NG - DX12",
        style,
        x, y, windowWidth, windowHeight,
        NULL, NULL, g_hInstance, NULL
    );
    
    if (!g_hwnd) {
        Com_Error(ERR_FATAL, "Failed to create window");
    }
    
    // Set up high DPI awareness
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
}

void Win32_ShutdownWindow(void) {
    if (g_hwnd) {
        DestroyWindow(g_hwnd);
        g_hwnd = NULL;
    }
    UnregisterClassW(L"VkQ3NG_DX12", g_hInstance);
}

void Win32_ProcessEvents(void) {
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            g_running = FALSE;
            break;
        }
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void Win32_SetWindowTitle(const char* title) {
    wchar_t wtitle[256];
    size_t converted;
    mbstowcs_s(&converted, wtitle, title, 256);
    SetWindowTextW(g_hwnd, wtitle);
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            g_running = FALSE;
            return 0;
            
        case WM_SIZE:
            if (wParam != SIZE_MINIMIZED) {
                int width = LOWORD(lParam);
                int height = HIWORD(lParam);
                if (width > 0 && height > 0) {
                    DX12_ResizeSwapchain(width, height);
                }
            }
            return 0;
            
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
            if (wParam < 512) {
                DX12_KeyDown((int)wParam);
            }
            if (wParam == VK_ESCAPE) {
                PostQuitMessage(0);
                g_running = FALSE;
            }
            return 0;
            
        case WM_KEYUP:
        case WM_SYSKEYUP:
            if (wParam < 512) {
                DX12_KeyUp((int)wParam);
            }
            return 0;
            
        case WM_MOUSEMOVE:
            DX12_MouseMove(LOWORD(lParam), HIWORD(lParam));
            return 0;
            
        case WM_LBUTTONDOWN:
            DX12_MouseButtonDown(0);
            SetCapture(hwnd);
            return 0;
            
        case WM_LBUTTONUP:
            DX12_MouseButtonUp(0);
            ReleaseCapture();
            return 0;
            
        case WM_RBUTTONDOWN:
            DX12_MouseButtonDown(1);
            SetCapture(hwnd);
            return 0;
            
        case WM_RBUTTONUP:
            DX12_MouseButtonUp(1);
            ReleaseCapture();
            return 0;
            
        case WM_MBUTTONDOWN:
            DX12_MouseButtonDown(2);
            SetCapture(hwnd);
            return 0;
            
        case WM_MBUTTONUP:
            DX12_MouseButtonUp(2);
            ReleaseCapture();
            return 0;
            
        case WM_MOUSEWHEEL:
            DX12_MouseWheel(GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA);
            return 0;
            
        case WM_GETMINMAXINFO:
            {
                MINMAXINFO* mmi = (MINMAXINFO*)lParam;
                mmi->ptMinTrackSize.x = 640;
                mmi->ptMinTrackSize.y = 480;
            }
            return 0;
            
        case WM_ACTIVATEAPP:
            // Handle focus loss/gain
            return 0;
            
        case WM_SETCURSOR:
            // Hide cursor in game
            if (LOWORD(lParam) == HTCLIENT) {
                SetCursor(NULL);
                return TRUE;
            }
            break;
    }
    
    return DefWindowProc(hwnd, msg, wParam, lParam);
}