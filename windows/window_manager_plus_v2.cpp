// This must be included before many other Windows headers.
#pragma once

#include <Windows.h>

#include <flutter/method_channel.h>
#include <flutter/plugin_registrar_windows.h>
#include <flutter/standard_method_codec.h>

#include <codecvt>
#include <dwmapi.h>
#include <map>
#include <memory>
#include <sstream>

#include "window_manager_plus_v2.h"

#include <algorithm>

#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "Gdi32.lib")

/// Window attribute that enables dark mode window decorations.
///
/// Redefined in case the developer's machine has a Windows SDK older than
/// version 10.0.22000.0.
/// See:
/// https://docs.microsoft.com/windows/win32/api/dwmapi/ne-dwmapi-dwmwindowattribute
#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif

constexpr const wchar_t kWindowClassName[] = L"FLUTTER_RUNNER_WIN32_WINDOW";

/// Registry key for app theme preference.
///
/// A value of 0 indicates apps should use dark mode. A non-zero or missing
/// value indicates apps should use light mode.
constexpr const wchar_t kGetPreferredBrightnessRegKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize";
constexpr const wchar_t kGetPreferredBrightnessRegValue[] =
    L"AppsUseLightTheme";

#define APPBAR_CALLBACK WM_USER + 0x01;

namespace window_manager_plus_v2 {

WindowManagerPlusPluginWindowCreatedCallback g_window_created_callback =
    nullptr;

const flutter::EncodableValue* ValueOrNull(const flutter::EncodableMap& map,
                                           const char* key) {
  auto it = map.find(flutter::EncodableValue(key));
  if (it == map.end()) {
    return nullptr;
  }
  return &(it->second);
}

WindowManagerPlus::WindowManagerPlus() {}

WindowManagerPlus::~WindowManagerPlus() {
#ifndef NDEBUG
  std::cout << "WindowManager dealloc" << std::endl;
#endif
}

int64_t WindowManagerPlus::createWindow(const std::vector<std::string>& args) {
  if (g_window_created_callback) {
    WindowManagerPlus::autoincrementId_++;
    auto windowId = WindowManagerPlus::autoincrementId_;
    std::vector<std::string> v1 = {std::to_string(windowId)};

    // add the windowId as first argument to command_line_arguments
    std::vector<std::string> dst;
    std::merge(v1.begin(), v1.end(), args.begin(), args.end(),
               std::back_inserter(dst));
    auto fWindow = g_window_created_callback(std::move(dst));
    WindowManagerPlus::windows_.insert({windowId, std::move(fWindow)});
    return windowId;
  }
  return -1;
}

HWND WindowManagerPlus::GetMainWindow() {
  return native_window;
}

void WindowManagerPlus::ForceRefresh() {
  HWND hWnd = GetMainWindow();

  RECT rect;

  GetWindowRect(hWnd, &rect);
  SetWindowPos(
      hWnd, nullptr, rect.left, rect.top, rect.right - rect.left + 1,
      rect.bottom - rect.top,
      SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);
  SetWindowPos(
      hWnd, nullptr, rect.left, rect.top, rect.right - rect.left,
      rect.bottom - rect.top,
      SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);
}

void WindowManagerPlus::ForceChildRefresh() {
  HWND hWnd = GetWindow(GetMainWindow(), GW_CHILD);

  RECT rect;

  GetWindowRect(hWnd, &rect);
  SetWindowPos(
      hWnd, nullptr, rect.left, rect.top, rect.right - rect.left + 1,
      rect.bottom - rect.top,
      SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);
  SetWindowPos(
      hWnd, nullptr, rect.left, rect.top, rect.right - rect.left,
      rect.bottom - rect.top,
      SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_FRAMECHANGED);
}

void WindowManagerPlus::SetAsFrameless() {
  is_frameless_ = true;
  HWND hWnd = GetMainWindow();

  RECT rect;

  GetWindowRect(hWnd, &rect);
  SetWindowPos(hWnd, nullptr, rect.left, rect.top, rect.right - rect.left,
               rect.bottom - rect.top,
               SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE |
                   SWP_FRAMECHANGED);
}

void WindowManagerPlus::WaitUntilReadyToShow() {
  ::CoCreateInstance(CLSID_TaskbarList, NULL, CLSCTX_INPROC_SERVER,
                     IID_PPV_ARGS(&taskbar_));
}

void WindowManagerPlus::Destroy() {
  PostQuitMessage(0);
}

void WindowManagerPlus::Close() {
  HWND hWnd = GetMainWindow();
  PostMessage(hWnd, WM_SYSCOMMAND, SC_CLOSE, 0);
}

void WindowManagerPlus::SetPreventClose(const flutter::EncodableMap& args) {
  is_prevent_close_ =
      std::get<bool>(args.at(flutter::EncodableValue("isPreventClose")));
}

bool WindowManagerPlus::IsPreventClose() {
  return is_prevent_close_;
}

void WindowManagerPlus::Focus() {
  HWND hWnd = GetMainWindow();
  if (IsMinimized()) {
    Restore();
  }

  ::SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
  SetForegroundWindow(hWnd);
}

void WindowManagerPlus::Blur() {
  HWND hWnd = GetMainWindow();
  HWND next_hwnd = ::GetNextWindow(hWnd, GW_HWNDNEXT);
  while (next_hwnd) {
    if (::IsWindowVisible(next_hwnd)) {
      ::SetForegroundWindow(next_hwnd);
      return;
    }
    next_hwnd = ::GetNextWindow(next_hwnd, GW_HWNDNEXT);
  }
}

bool WindowManagerPlus::IsFocused() {
  return GetMainWindow() == GetForegroundWindow();
}

void WindowManagerPlus::Show() {
  HWND hWnd = GetMainWindow();
  DWORD gwlStyle = GetWindowLong(hWnd, GWL_STYLE);
  gwlStyle = gwlStyle | WS_VISIBLE;
  if ((gwlStyle & WS_VISIBLE) == 0) {
    SetWindowLong(hWnd, GWL_STYLE, gwlStyle);
    ::SetWindowPos(hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
  }

  ShowWindowAsync(GetMainWindow(), SW_SHOW);
  SetForegroundWindow(GetMainWindow());
}

void WindowManagerPlus::Hide() {
  ShowWindow(GetMainWindow(), SW_HIDE);
}

bool WindowManagerPlus::IsVisible() {
  bool isVisible = IsWindowVisible(GetMainWindow());
  return isVisible;
}

bool WindowManagerPlus::IsMaximized() {
  HWND mainWindow = GetMainWindow();
  WINDOWPLACEMENT windowPlacement;
  GetWindowPlacement(mainWindow, &windowPlacement);

  return windowPlacement.showCmd == SW_MAXIMIZE;
}

void WindowManagerPlus::Maximize(const flutter::EncodableMap& args) {
  bool vertically =
      std::get<bool>(args.at(flutter::EncodableValue("vertically")));

  HWND hwnd = GetMainWindow();
  WINDOWPLACEMENT windowPlacement;
  GetWindowPlacement(hwnd, &windowPlacement);

  if (vertically) {
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    PostMessage(hwnd, WM_NCLBUTTONDBLCLK, HTTOP,
                MAKELPARAM(cursorPos.x, cursorPos.y));
  } else {
    if (windowPlacement.showCmd != SW_MAXIMIZE) {
      PostMessage(hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    }
  }
}

void WindowManagerPlus::Unmaximize() {
  HWND mainWindow = GetMainWindow();
  WINDOWPLACEMENT windowPlacement;
  GetWindowPlacement(mainWindow, &windowPlacement);

  if (windowPlacement.showCmd != SW_NORMAL) {
    PostMessage(mainWindow, WM_SYSCOMMAND, SC_RESTORE, 0);
  }
}

bool WindowManagerPlus::IsMinimized() {
  HWND mainWindow = GetMainWindow();
  WINDOWPLACEMENT windowPlacement;
  GetWindowPlacement(mainWindow, &windowPlacement);

  return windowPlacement.showCmd == SW_SHOWMINIMIZED;
}

void WindowManagerPlus::Minimize() {
  if (IsFullScreen()) {  // Like chromium, we don't want to minimize fullscreen
                         // windows
    return;
  }
  HWND mainWindow = GetMainWindow();
  WINDOWPLACEMENT windowPlacement;
  GetWindowPlacement(mainWindow, &windowPlacement);

  if (windowPlacement.showCmd != SW_SHOWMINIMIZED) {
    PostMessage(mainWindow, WM_SYSCOMMAND, SC_MINIMIZE, 0);
  }
}

void WindowManagerPlus::Restore() {
  HWND mainWindow = GetMainWindow();
  WINDOWPLACEMENT windowPlacement;
  GetWindowPlacement(mainWindow, &windowPlacement);

  if (windowPlacement.showCmd != SW_NORMAL) {
    PostMessage(mainWindow, WM_SYSCOMMAND, SC_RESTORE, 0);
  }
}

bool WindowManagerPlus::IsDockable() {
  return true;
}

int WindowManagerPlus::IsDocked() {
  return is_docked_;
}

double WindowManagerPlus::GetDpiForHwnd(HWND hWnd) {
  auto monitor = MonitorFromWindow(hWnd, MONITOR_DEFAULTTONEAREST);
  UINT newDpiX = 96;  // Default values
  UINT newDpiY = 96;

  // Dynamically load shcore.dll and get the GetDpiForMonitor function address
  // We need to do this to ensure Windows 7 support
  HMODULE shcore = LoadLibrary(TEXT("shcore.dll"));
  if (shcore) {
    typedef HRESULT (*GetDpiForMonitor)(HMONITOR, int, UINT*, UINT*);

    GetDpiForMonitor GetDpiForMonitorFunc =
        (GetDpiForMonitor)GetProcAddress(shcore, "GetDpiForMonitor");

    if (GetDpiForMonitorFunc) {
      // Use the loaded function if available
      const int MDT_EFFECTIVE_DPI = 0;
      if (FAILED(GetDpiForMonitorFunc(monitor, MDT_EFFECTIVE_DPI, &newDpiX,
                                      &newDpiY))) {
        // If it fails, set the default values again
        newDpiX = 96;
        newDpiY = 96;
      }
    }
    FreeLibrary(shcore);
  }
  return ((double)newDpiX);
}

void WindowManagerPlus::Dock(const flutter::EncodableMap& args) {
  HWND mainWindow = GetMainWindow();

  double dpi = GetDpiForHwnd(mainWindow);
  double scalingFactor = dpi / 96.0;

  bool left = std::get<bool>(args.at(flutter::EncodableValue("left")));
  bool right = std::get<bool>(args.at(flutter::EncodableValue("right")));
  int width = std::get<int>(args.at(flutter::EncodableValue("width")));

  // first register bar
  RegisterAccessBar(mainWindow, true);

  //
  UINT edge = ABE_LEFT;
  if (right && !left) {
    edge = ABE_RIGHT;
  }

  UINT uw = static_cast<UINT>(width * scalingFactor + 0.5);

  // dock window
  DockAccessBar(mainWindow, edge, uw);
}

bool WindowManagerPlus::Undock() {
  HWND mainWindow = GetMainWindow();
  bool result = RegisterAccessBar(mainWindow, false);
  is_docked_ = 0;
  return result;
}

void PASCAL WindowManagerPlus::AppBarQuerySetPos(HWND hwnd,
                                                 UINT uEdge,
                                                 LPRECT lprc,
                                                 PAPPBARDATA pabd) {
  int iHeight = 0;
  int iWidth = 0;

  pabd->hWnd = hwnd;
  pabd->rc = *lprc;
  pabd->uEdge = uEdge;

  // Copy the screen coordinates of the appbar's bounding
  // rectangle into the APPBARDATA structure.
  if ((uEdge == ABE_LEFT) || (uEdge == ABE_RIGHT)) {
    iWidth = pabd->rc.right - pabd->rc.left;
    pabd->rc.top = 0;
    pabd->rc.bottom = GetSystemMetrics(SM_CYSCREEN);
  } else {
    iHeight = pabd->rc.bottom - pabd->rc.top;
    pabd->rc.left = 0;
    pabd->rc.right = GetSystemMetrics(SM_CXSCREEN);
  }

  // Query the system for an approved size and position.
  SHAppBarMessage(ABM_QUERYPOS, pabd);

  // Adjust the rectangle, depending on the edge to which the appbar is
  // anchored.
  switch (uEdge) {
    case ABE_LEFT:
      pabd->rc.right = pabd->rc.left + iWidth;
      break;

    case ABE_RIGHT:
      pabd->rc.left = pabd->rc.right - iWidth;
      break;

    case ABE_TOP:
      pabd->rc.bottom = pabd->rc.top + iHeight;
      break;

    case ABE_BOTTOM:
      pabd->rc.top = pabd->rc.bottom - iHeight;
      break;
  }

  // Pass the final bounding rectangle to the system.
  SHAppBarMessage(ABM_SETPOS, pabd);

  // Move and size the appbar so that it conforms to the
  // bounding rectangle passed to the system.
  UINT uFlags = NULL;
  SetWindowPos(hwnd, HWND_TOP, pabd->rc.left, pabd->rc.top,
               pabd->rc.right - pabd->rc.left, pabd->rc.bottom - pabd->rc.top,
               uFlags);
}

BOOL WindowManagerPlus::RegisterAccessBar(HWND hwnd, BOOL fRegister) {
  APPBARDATA abd;

  // Specify the structure size and handle to the appbar.
  abd.cbSize = sizeof(APPBARDATA);
  abd.hWnd = hwnd;

  if (fRegister && is_registered_for_docking_) {
    return false;
  }

  if (!fRegister && !is_registered_for_docking_) {
    return false;
  }

  if (!fRegister && is_registered_for_docking_) {
    // Unregister the appbar.
    SHAppBarMessage(ABM_REMOVE, &abd);
    is_registered_for_docking_ = false;
    is_docked_ = 0;
    return true;
  }

  if (fRegister && !is_registered_for_docking_) {
    // Provide an identifier for notification messages.
    abd.uCallbackMessage = APPBAR_CALLBACK;

    // Register the appbar.
    if (!SHAppBarMessage(ABM_NEW, &abd))
      return false;

    is_docked_ = 1;  // default edge
    is_registered_for_docking_ = true;
    return false;
  }

  return false;
}

void WindowManagerPlus::DockAccessBar(HWND hwnd, UINT edge, UINT windowWidth) {
  APPBARDATA abd;
  RECT lprc;

  lprc.top = 0;
  lprc.bottom = 0;

  if (edge == ABE_LEFT) {
    lprc.left = 0;
    lprc.right = windowWidth;
  } else {
    lprc.left = GetSystemMetrics(SM_CXSCREEN) - windowWidth;
    lprc.right = GetSystemMetrics(SM_CXSCREEN);
  }

  // Specify the structure size and handle to the appbar.
  abd.cbSize = sizeof(APPBARDATA);
  abd.hWnd = hwnd;
  abd.uCallbackMessage = APPBAR_CALLBACK;

  AppBarQuerySetPos(hwnd, edge, &lprc, &abd);

  if (edge == ABE_LEFT) {
    is_docked_ = 1;
  } else if (edge == ABE_RIGHT) {
    is_docked_ = 2;
  }

  return;
}

bool WindowManagerPlus::IsFullScreen() {
  return g_is_window_fullscreen;
}

void WindowManagerPlus::SetFullScreen(const flutter::EncodableMap& args) {
  bool isFullScreen =
      std::get<bool>(args.at(flutter::EncodableValue("isFullScreen")));

  HWND mainWindow = GetMainWindow();

  // Previously inspired by how Chromium does this
  // https://src.chromium.org/viewvc/chrome/trunk/src/ui/views/win/fullscreen_handler.cc?revision=247204&view=markup
  // Instead, we use a modified implementation of how the media_kit package
  // implements this (we got permission from the author, I believe)
  // https://github.com/alexmercerind/media_kit/blob/1226bcff36eab27cb17d60c33e9c15ca489c1f06/media_kit_video/windows/utils.cc

  // Save current window state if not already fullscreen.
  if (!g_is_window_fullscreen) {
    // Save current window information.
    g_maximized_before_fullscreen = ::IsZoomed(mainWindow);
    g_style_before_fullscreen = GetWindowLong(mainWindow, GWL_STYLE);
    ::GetWindowRect(mainWindow, &g_frame_before_fullscreen);
    g_title_bar_style_before_fullscreen = title_bar_style_;
  }

  g_is_window_fullscreen = isFullScreen;

  if (isFullScreen) {  // Set to fullscreen
    ::SendMessage(mainWindow, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    if (!is_frameless_) {
      auto monitor = MONITORINFO{};
      auto placement = WINDOWPLACEMENT{};
      monitor.cbSize = sizeof(MONITORINFO);
      placement.length = sizeof(WINDOWPLACEMENT);
      ::GetWindowPlacement(mainWindow, &placement);
      ::GetMonitorInfo(
          ::MonitorFromWindow(mainWindow, MONITOR_DEFAULTTONEAREST), &monitor);
      ::SetWindowLongPtr(mainWindow, GWL_STYLE,
                         g_style_before_fullscreen & ~WS_OVERLAPPEDWINDOW);
      ::SetWindowPos(mainWindow, HWND_TOP, monitor.rcMonitor.left,
                     monitor.rcMonitor.top,
                     monitor.rcMonitor.right - monitor.rcMonitor.left,
                     monitor.rcMonitor.bottom - monitor.rcMonitor.top,
                     SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
    }
  } else {  // Restore from fullscreen
    if (!g_maximized_before_fullscreen)
      Restore();
    ::SetWindowLongPtr(mainWindow, GWL_STYLE,
                       g_style_before_fullscreen | WS_OVERLAPPEDWINDOW);
    if (::IsZoomed(mainWindow)) {
      // Refresh the parent mainWindow.
      ::SetWindowPos(mainWindow, nullptr, 0, 0, 0, 0,
                     SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER |
                         SWP_FRAMECHANGED);
      auto rect = RECT{};
      ::GetClientRect(mainWindow, &rect);
      auto flutter_view = ::FindWindowEx(mainWindow, nullptr,
                                         kFlutterViewWindowClassName, nullptr);
      ::SetWindowPos(flutter_view, nullptr, rect.left, rect.top,
                     rect.right - rect.left, rect.bottom - rect.top,
                     SWP_NOACTIVATE | SWP_NOZORDER);
      if (g_maximized_before_fullscreen)
        PostMessage(mainWindow, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    } else {
      ::SetWindowPos(
          mainWindow, nullptr, g_frame_before_fullscreen.left,
          g_frame_before_fullscreen.top,
          g_frame_before_fullscreen.right - g_frame_before_fullscreen.left,
          g_frame_before_fullscreen.bottom - g_frame_before_fullscreen.top,
          SWP_NOACTIVATE | SWP_NOZORDER);
    }
  }
}

void WindowManagerPlus::SetAspectRatio(const flutter::EncodableMap& args) {
  aspect_ratio_ =
      std::get<double>(args.at(flutter::EncodableValue("aspectRatio")));
}

void WindowManagerPlus::SetBackgroundColor(const flutter::EncodableMap& args) {
  int backgroundColorA =
      std::get<int>(args.at(flutter::EncodableValue("backgroundColorA")));
  int backgroundColorR =
      std::get<int>(args.at(flutter::EncodableValue("backgroundColorR")));
  int backgroundColorG =
      std::get<int>(args.at(flutter::EncodableValue("backgroundColorG")));
  int backgroundColorB =
      std::get<int>(args.at(flutter::EncodableValue("backgroundColorB")));

  bool isTransparent = backgroundColorA == 0 && backgroundColorR == 0 &&
                       backgroundColorG == 0 && backgroundColorB == 0;

  HWND hWnd = GetMainWindow();
  const HINSTANCE hModule = LoadLibrary(TEXT("user32.dll"));
  if (hModule) {
    typedef enum _ACCENT_STATE {
      ACCENT_DISABLED = 0,
      ACCENT_ENABLE_GRADIENT = 1,
      ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
      ACCENT_ENABLE_BLURBEHIND = 3,
      ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,
      ACCENT_ENABLE_HOSTBACKDROP = 5,
      ACCENT_INVALID_STATE = 6
    } ACCENT_STATE;
    struct ACCENTPOLICY {
      int nAccentState;
      int nFlags;
      int nColor;
      int nAnimationId;
    };
    struct WINCOMPATTRDATA {
      int nAttribute;
      PVOID pData;
      ULONG ulDataSize;
    };
    typedef BOOL(WINAPI * pSetWindowCompositionAttribute)(HWND,
                                                          WINCOMPATTRDATA*);
    const pSetWindowCompositionAttribute SetWindowCompositionAttribute =
        (pSetWindowCompositionAttribute)GetProcAddress(
            hModule, "SetWindowCompositionAttribute");
    if (SetWindowCompositionAttribute) {
      int32_t accent_state = isTransparent ? ACCENT_ENABLE_TRANSPARENTGRADIENT
                                           : ACCENT_ENABLE_GRADIENT;
      ACCENTPOLICY policy = {
          accent_state, 2,
          ((backgroundColorA << 24) + (backgroundColorB << 16) +
           (backgroundColorG << 8) + (backgroundColorR)),
          0};
      WINCOMPATTRDATA data = {19, &policy, sizeof(policy)};
      SetWindowCompositionAttribute(hWnd, &data);
    }
    FreeLibrary(hModule);
  }
}

flutter::EncodableMap WindowManagerPlus::GetBounds(
    const flutter::EncodableMap& args) {
  HWND hwnd = GetMainWindow();
  double devicePixelRatio =
      std::get<double>(args.at(flutter::EncodableValue("devicePixelRatio")));

  flutter::EncodableMap resultMap = flutter::EncodableMap();
  RECT rect;
  if (GetWindowRect(hwnd, &rect)) {
    double x = rect.left / devicePixelRatio * 1.0f;
    double y = rect.top / devicePixelRatio * 1.0f;
    double width = (rect.right - rect.left) / devicePixelRatio * 1.0f;
    double height = (rect.bottom - rect.top) / devicePixelRatio * 1.0f;

    resultMap[flutter::EncodableValue("x")] = flutter::EncodableValue(x);
    resultMap[flutter::EncodableValue("y")] = flutter::EncodableValue(y);
    resultMap[flutter::EncodableValue("width")] =
        flutter::EncodableValue(width);
    resultMap[flutter::EncodableValue("height")] =
        flutter::EncodableValue(height);
  }
  return resultMap;
}

void WindowManagerPlus::SetBounds(const flutter::EncodableMap& args) {
  HWND hwnd = GetMainWindow();

  double devicePixelRatio =
      std::get<double>(args.at(flutter::EncodableValue("devicePixelRatio")));

  auto* null_or_x = std::get_if<double>(ValueOrNull(args, "x"));
  auto* null_or_y = std::get_if<double>(ValueOrNull(args, "y"));
  auto* null_or_width = std::get_if<double>(ValueOrNull(args, "width"));
  auto* null_or_height = std::get_if<double>(ValueOrNull(args, "height"));

  int x = 0;
  int y = 0;
  int width = 0;
  int height = 0;
  UINT uFlags = NULL;

  if (null_or_x != nullptr && null_or_y != nullptr) {
    x = static_cast<int>(*null_or_x * devicePixelRatio);
    y = static_cast<int>(*null_or_y * devicePixelRatio);
  }
  if (null_or_width != nullptr && null_or_height != nullptr) {
    width = static_cast<int>(*null_or_width * devicePixelRatio);
    height = static_cast<int>(*null_or_height * devicePixelRatio);
  }

  if (null_or_x == nullptr || null_or_y == nullptr) {
    uFlags = SWP_NOMOVE;
  }
  if (null_or_width == nullptr || null_or_height == nullptr) {
    uFlags = SWP_NOSIZE;
  }

  SetWindowPos(hwnd, HWND_TOP, x, y, width, height, uFlags);
}

void WindowManagerPlus::SetMinimumSize(const flutter::EncodableMap& args) {
  double devicePixelRatio =
      std::get<double>(args.at(flutter::EncodableValue("devicePixelRatio")));
  double width = std::get<double>(args.at(flutter::EncodableValue("width")));
  double height = std::get<double>(args.at(flutter::EncodableValue("height")));

  if (width >= 0 && height >= 0) {
    pixel_ratio_ = devicePixelRatio;
    POINT point = {};
    point.x = static_cast<LONG>(width);
    point.y = static_cast<LONG>(height);
    minimum_size_ = point;
  }
}

void WindowManagerPlus::SetMaximumSize(const flutter::EncodableMap& args) {
  double devicePixelRatio =
      std::get<double>(args.at(flutter::EncodableValue("devicePixelRatio")));
  double width = std::get<double>(args.at(flutter::EncodableValue("width")));
  double height = std::get<double>(args.at(flutter::EncodableValue("height")));

  if (width >= 0 && height >= 0) {
    pixel_ratio_ = devicePixelRatio;
    POINT point = {};
    point.x = static_cast<LONG>(width);
    point.y = static_cast<LONG>(height);
    maximum_size_ = point;
  }
}

bool WindowManagerPlus::IsResizable() {
  return is_resizable_;
}

void WindowManagerPlus::SetResizable(const flutter::EncodableMap& args) {
  HWND hWnd = GetMainWindow();
  is_resizable_ =
      std::get<bool>(args.at(flutter::EncodableValue("isResizable")));
  DWORD gwlStyle = GetWindowLong(hWnd, GWL_STYLE);
  if (is_resizable_) {
    gwlStyle |= WS_THICKFRAME;
  } else {
    gwlStyle &= ~WS_THICKFRAME;
  }
  ::SetWindowLong(hWnd, GWL_STYLE, gwlStyle);
}

bool WindowManagerPlus::IsMinimizable() {
  HWND hWnd = GetMainWindow();
  DWORD gwlStyle = GetWindowLong(hWnd, GWL_STYLE);
  return (gwlStyle & WS_MINIMIZEBOX) != 0;
}

void WindowManagerPlus::SetMinimizable(const flutter::EncodableMap& args) {
  HWND hWnd = GetMainWindow();
  bool isMinimizable =
      std::get<bool>(args.at(flutter::EncodableValue("isMinimizable")));
  DWORD gwlStyle = GetWindowLong(hWnd, GWL_STYLE);
  gwlStyle =
      isMinimizable ? gwlStyle | WS_MINIMIZEBOX : gwlStyle & ~WS_MINIMIZEBOX;
  SetWindowLong(hWnd, GWL_STYLE, gwlStyle);
}

bool WindowManagerPlus::IsMaximizable() {
  HWND hWnd = GetMainWindow();
  DWORD gwlStyle = GetWindowLong(hWnd, GWL_STYLE);
  return (gwlStyle & WS_MAXIMIZEBOX) != 0;
}

void WindowManagerPlus::SetMaximizable(const flutter::EncodableMap& args) {
  HWND hWnd = GetMainWindow();
  bool isMaximizable =
      std::get<bool>(args.at(flutter::EncodableValue("isMaximizable")));
  DWORD gwlStyle = GetWindowLong(hWnd, GWL_STYLE);
  gwlStyle =
      isMaximizable ? gwlStyle | WS_MAXIMIZEBOX : gwlStyle & ~WS_MAXIMIZEBOX;
  SetWindowLong(hWnd, GWL_STYLE, gwlStyle);
}

bool WindowManagerPlus::IsClosable() {
  HWND hWnd = GetMainWindow();
  DWORD gclStyle = GetClassLong(hWnd, GCL_STYLE);
  return !((gclStyle & CS_NOCLOSE) != 0);
}

void WindowManagerPlus::SetClosable(const flutter::EncodableMap& args) {
  HWND hWnd = GetMainWindow();
  bool isClosable =
      std::get<bool>(args.at(flutter::EncodableValue("isClosable")));
  DWORD gclStyle = GetClassLong(hWnd, GCL_STYLE);
  gclStyle = isClosable ? gclStyle & ~CS_NOCLOSE : gclStyle | CS_NOCLOSE;
  SetClassLong(hWnd, GCL_STYLE, gclStyle);
}

bool WindowManagerPlus::IsAlwaysOnTop() {
  DWORD dwExStyle = GetWindowLong(GetMainWindow(), GWL_EXSTYLE);
  return (dwExStyle & WS_EX_TOPMOST) != 0;
}

void WindowManagerPlus::SetAlwaysOnTop(const flutter::EncodableMap& args) {
  bool isAlwaysOnTop =
      std::get<bool>(args.at(flutter::EncodableValue("isAlwaysOnTop")));
  SetWindowPos(GetMainWindow(), isAlwaysOnTop ? HWND_TOPMOST : HWND_NOTOPMOST,
               0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

bool WindowManagerPlus::IsAlwaysOnBottom() {
  return is_always_on_bottom_;
}

void WindowManagerPlus::SetAlwaysOnBottom(const flutter::EncodableMap& args) {
  is_always_on_bottom_ =
      std::get<bool>(args.at(flutter::EncodableValue("isAlwaysOnBottom")));

  SetWindowPos(GetMainWindow(),
               is_always_on_bottom_ ? HWND_BOTTOM : HWND_NOTOPMOST, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_NOSIZE);
}

std::string WindowManagerPlus::GetTitle() {
  int const bufferSize = 1 + GetWindowTextLength(GetMainWindow());
  std::wstring title(bufferSize, L'\0');
  GetWindowText(GetMainWindow(), &title[0], bufferSize);

  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  return (converter.to_bytes(title)).c_str();
}

void WindowManagerPlus::SetTitle(const flutter::EncodableMap& args) {
  std::string title =
      std::get<std::string>(args.at(flutter::EncodableValue("title")));

  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
  SetWindowText(GetMainWindow(), converter.from_bytes(title).c_str());
}

void WindowManagerPlus::SetTitleBarStyle(const flutter::EncodableMap& args) {
  title_bar_style_ =
      std::get<std::string>(args.at(flutter::EncodableValue("titleBarStyle")));
  // Enables the ability to go from setAsFrameless() to
  // TitleBarStyle.normal/hidden
  is_frameless_ = false;

  MARGINS margins = {0, 0, 0, 0};
  HWND hWnd = GetMainWindow();
  RECT rect;
  GetWindowRect(hWnd, &rect);
  DwmExtendFrameIntoClientArea(hWnd, &margins);
  SetWindowPos(hWnd, nullptr, rect.left, rect.top, 0, 0,
               SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_NOMOVE | SWP_NOSIZE |
                   SWP_FRAMECHANGED);
}

int WindowManagerPlus::GetTitleBarHeight() {
  HWND hWnd = GetMainWindow();

  TITLEBARINFOEX* ptinfo = (TITLEBARINFOEX*)malloc(sizeof(TITLEBARINFOEX));
  ptinfo->cbSize = sizeof(TITLEBARINFOEX);
  SendMessage(hWnd, WM_GETTITLEBARINFOEX, 0, (LPARAM)ptinfo);
  int height = ptinfo->rcTitleBar.bottom == 0
                   ? 0
                   : ptinfo->rcTitleBar.bottom - ptinfo->rcTitleBar.top;
  free(ptinfo);

  return height;
}

bool WindowManagerPlus::IsSkipTaskbar() {
  return is_skip_taskbar_;
}

void WindowManagerPlus::SetSkipTaskbar(const flutter::EncodableMap& args) {
  is_skip_taskbar_ =
      std::get<bool>(args.at(flutter::EncodableValue("isSkipTaskbar")));

  HWND hWnd = GetMainWindow();

  LPVOID lp = NULL;
  CoInitialize(lp);

  taskbar_->HrInit();
  if (!is_skip_taskbar_)
    taskbar_->AddTab(hWnd);
  else
    taskbar_->DeleteTab(hWnd);
}

void WindowManagerPlus::SetProgressBar(const flutter::EncodableMap& args) {
  double progress =
      std::get<double>(args.at(flutter::EncodableValue("progress")));

  HWND hWnd = GetMainWindow();
  taskbar_->SetProgressState(hWnd, TBPF_INDETERMINATE);
  taskbar_->SetProgressValue(hWnd, static_cast<int32_t>(progress * 100),
                             static_cast<int32_t>(100));

  if (progress < 0) {
    taskbar_->SetProgressState(hWnd, TBPF_NOPROGRESS);
    taskbar_->SetProgressValue(hWnd, static_cast<int32_t>(0),
                               static_cast<int32_t>(0));
  } else if (progress > 1) {
    taskbar_->SetProgressState(hWnd, TBPF_INDETERMINATE);
    taskbar_->SetProgressValue(hWnd, static_cast<int32_t>(100),
                               static_cast<int32_t>(100));
  } else {
    taskbar_->SetProgressState(hWnd, TBPF_INDETERMINATE);
    taskbar_->SetProgressValue(hWnd, static_cast<int32_t>(progress * 100),
                               static_cast<int32_t>(100));
  }
}

void WindowManagerPlus::SetIcon(const flutter::EncodableMap& args) {
  std::string iconPath =
      std::get<std::string>(args.at(flutter::EncodableValue("iconPath")));

  std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;

  HICON hIconSmall =
      (HICON)(LoadImage(NULL, (LPCWSTR)(converter.from_bytes(iconPath).c_str()),
                        IMAGE_ICON, 16, 16, LR_LOADFROMFILE));

  HICON hIconLarge =
      (HICON)(LoadImage(NULL, (LPCWSTR)(converter.from_bytes(iconPath).c_str()),
                        IMAGE_ICON, 32, 32, LR_LOADFROMFILE));

  HWND hWnd = GetMainWindow();

  SendMessage(hWnd, WM_SETICON, ICON_SMALL, (LPARAM)hIconSmall);
  SendMessage(hWnd, WM_SETICON, ICON_BIG, (LPARAM)hIconLarge);
}

bool WindowManagerPlus::HasShadow() {
  if (is_frameless_)
    return has_shadow_;
  return true;
}

void WindowManagerPlus::SetHasShadow(const flutter::EncodableMap& args) {
  if (is_frameless_) {
    has_shadow_ = std::get<bool>(args.at(flutter::EncodableValue("hasShadow")));

    HWND hWnd = GetMainWindow();

    MARGINS margins[2]{{0, 0, 0, 0}, {0, 0, 1, 0}};

    DwmExtendFrameIntoClientArea(hWnd, &margins[has_shadow_]);
  }
}

double WindowManagerPlus::GetOpacity() {
  return opacity_;
}

void WindowManagerPlus::SetOpacity(const flutter::EncodableMap& args) {
  opacity_ = std::get<double>(args.at(flutter::EncodableValue("opacity")));
  HWND hWnd = GetMainWindow();
  long gwlExStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
  SetWindowLong(hWnd, GWL_EXSTYLE, gwlExStyle | WS_EX_LAYERED);
  SetLayeredWindowAttributes(hWnd, 0, static_cast<int8_t>(255 * opacity_),
                             0x02);
}

void WindowManagerPlus::SetBrightness(const flutter::EncodableMap& args) {
  DWORD light_mode;
  DWORD light_mode_size = sizeof(light_mode);
  LSTATUS result =
      RegGetValue(HKEY_CURRENT_USER, kGetPreferredBrightnessRegKey,
                  kGetPreferredBrightnessRegValue, RRF_RT_REG_DWORD, nullptr,
                  &light_mode, &light_mode_size);

  if (result == ERROR_SUCCESS) {
    std::string brightness =
        std::get<std::string>(args.at(flutter::EncodableValue("brightness")));
    HWND hWnd = GetMainWindow();
    BOOL enable_dark_mode = light_mode == 0 && brightness == "dark";
    DwmSetWindowAttribute(hWnd, DWMWA_USE_IMMERSIVE_DARK_MODE,
                          &enable_dark_mode, sizeof(enable_dark_mode));
  }
}

void WindowManagerPlus::SetIgnoreMouseEvents(
    const flutter::EncodableMap& args) {
  bool ignore = std::get<bool>(args.at(flutter::EncodableValue("ignore")));

  HWND hwnd = GetMainWindow();
  LONG ex_style = ::GetWindowLong(hwnd, GWL_EXSTYLE);
  if (ignore)
    ex_style |= (WS_EX_TRANSPARENT | WS_EX_LAYERED);
  else
    ex_style &= ~(WS_EX_TRANSPARENT | WS_EX_LAYERED);

  ::SetWindowLong(hwnd, GWL_EXSTYLE, ex_style);
}

void WindowManagerPlus::PopUpWindowMenu(const flutter::EncodableMap& args) {
  HWND hWnd = GetMainWindow();
  HMENU hMenu = GetSystemMenu(hWnd, false);

  double x, y;

  POINT cursorPos;
  GetCursorPos(&cursorPos);
  x = cursorPos.x;
  y = cursorPos.y;

  int cmd =
      TrackPopupMenu(hMenu, TPM_LEFTBUTTON | TPM_RIGHTBUTTON | TPM_RETURNCMD,
                     static_cast<int>(x), static_cast<int>(y), 0, hWnd, NULL);

  if (cmd) {
    PostMessage(hWnd, WM_SYSCOMMAND, cmd, 0);
  }
}

void WindowManagerPlus::StartDragging() {
  ReleaseCapture();
  Undock();
  SendMessage(GetMainWindow(), WM_SYSCOMMAND, SC_MOVE | HTCAPTION, 0);
}

void WindowManagerPlus::StartResizing(const flutter::EncodableMap& args) {
  bool top = std::get<bool>(args.at(flutter::EncodableValue("top")));
  bool bottom = std::get<bool>(args.at(flutter::EncodableValue("bottom")));
  bool left = std::get<bool>(args.at(flutter::EncodableValue("left")));
  bool right = std::get<bool>(args.at(flutter::EncodableValue("right")));

  HWND hWnd = GetMainWindow();
  Undock();
  ReleaseCapture();
  LONG command;
  if (top && !bottom && !right && !left) {
    command = HTTOP;
  } else if (top && left && !bottom && !right) {
    command = HTTOPLEFT;
  } else if (left && !top && !bottom && !right) {
    command = HTLEFT;
  } else if (right && !top && !left && !bottom) {
    command = HTRIGHT;
  } else if (top && right && !left && !bottom) {
    command = HTTOPRIGHT;
  } else if (bottom && !top && !right && !left) {
    command = HTBOTTOM;
  } else if (bottom && left && !top && !right) {
    command = HTBOTTOMLEFT;
  } else
    command = HTBOTTOMRIGHT;
  POINT cursorPos;
  GetCursorPos(&cursorPos);
  PostMessage(hWnd, WM_NCLBUTTONDOWN, command,
              MAKELPARAM(cursorPos.x, cursorPos.y));
}

}  // namespace window_manager_plus_v2

void WindowManagerPlusPluginSetWindowCreatedCallback(
    WindowManagerPlusPluginWindowCreatedCallback callback) {
  window_manager_plus_v2::g_window_created_callback = callback;
}