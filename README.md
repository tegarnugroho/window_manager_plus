# window_manager_plus_v2

[![pub version][pub-image]][pub-url] [![All Contributors][all-contributors-image]](#contributors)

[pub-image]: https://img.shields.io/pub/v/window_manager_plus_v2.svg
[pub-url]: https://pub.dev/packages/window_manager_plus_v2
[all-contributors-image]: https://img.shields.io/github/all-contributors/pichillilorenzo/window_manager_plus_v2?color=ee8449&style=flat-square

This plugin allows Flutter desktop apps to create and manage multiple windows, such as resizing and repositioning, and communicate between them.

This is a fork and a re-work of the original [window_manager](https://pub.dev/packages/window_manager) plugin.
With inspiration from the [desktop_multi_window](https://pub.dev/packages/desktop_multi_window) plugin,
this new implementation allows the creation and management of multiple windows.

**Linux is not currently supported.**

---

- [Platform Support](#platform-support)
- [Quick Start](#quick-start)
  - [Setup to support multiple windows](#setup-to-support-multiple-windows)
    - [macOS](#macos)
    - [Windows](#windows)
  - [Usage](#usage)
    - [Create a new window](#create-a-new-window)
    - [Communication between windows](#communication-between-windows)
    - [Listening events](#listening-events)
    - [Quit on close](#quit-on-close)
      - [macOS](#macos-1)
      - [Windows](#windows-1)
    - [Confirm before closing](#confirm-before-closing)
    - [Hidden at launch](#hidden-at-launch)
      - [Linux](#linux)
      - [macOS](#macos-2)
      - [Windows](#windows-2)
- [Articles](#articles)
- [API](#api)
  - [WindowManagerPlus](#windowmanagerplus)
    - [Methods](#methods)
    - [Static Methods](#static-methods)
  - [WindowListener](#windowlistener)
    - [Methods](#methods-1)
- [Contributors](#contributors)
- [License](#license)

## Platform Support

| Linux | macOS | Windows |
|:-----:|:-----:|:-------:|
|   ❌   |   ✅   |   ✅️    |

## Quick Start

### Setup to support multiple windows

#### macOS

Change the file `macos/Runner/MainFlutterWindow.swift` as follows:

```diff
import Cocoa
import FlutterMacOS
+ import window_manager_plus_v2

class MainFlutterWindow: NSPanel {
    override func awakeFromNib() {
        let flutterViewController = FlutterViewController.init()
        let windowFrame = self.frame
        self.contentViewController = flutterViewController
        self.setFrame(windowFrame, display: true)
        
        RegisterGeneratedPlugins(registry: flutterViewController)
+        
+        WindowManagerPlusPlugin.RegisterGeneratedPlugins = RegisterGeneratedPlugins
        
        super.awakeFromNib()
    }
    
    override public func order(_ place: NSWindow.OrderingMode, relativeTo otherWin: Int) {
        super.order(place, relativeTo: otherWin)
        hiddenWindowAtLaunch()
    }
}
```

Change the file `macos/Runner/AppDelegate.swift` as follows:

```diff
import Cocoa
import FlutterMacOS
+ import window_manager_plus_v2

@main
class AppDelegate: FlutterAppDelegate {
  override func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
-    return true
+    return NSApp.windows.filter({$0 is MainFlutterWindow || $0 is WindowManagerPlusFlutterWindow}).count == 1 // or return false
  }
}
```

Without changing the return logic, the application will close when the main flutter window is closed.

#### Windows

Change the file `windows/runner/main.cpp` as follows:

```diff
#include <flutter/dart_project.h>
#include <flutter/flutter_view_controller.h>
#include <windows.h>

#include "flutter_window.h"
#include "utils.h"

+ #include <iostream>
+ #include "window_manager_plus_v2/window_manager_plus_v2_plugin.h"

int APIENTRY wWinMain(_In_ HINSTANCE instance,
                      _In_opt_ HINSTANCE prev,
                      _In_ wchar_t* command_line,
                      _In_ int show_command) {
  // Attach to console when present (e.g., 'flutter run') or create a
  // new console when running with a debugger.
  if (!::AttachConsole(ATTACH_PARENT_PROCESS) && ::IsDebuggerPresent()) {
    CreateAndAttachConsole();
  }

  // Initialize COM, so that it is available for use in the library and/or
  // plugins.
  ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  flutter::DartProject project(L"data");

  std::vector<std::string> command_line_arguments = GetCommandLineArguments();

  project.set_dart_entrypoint_arguments(std::move(command_line_arguments));

  FlutterWindow window(project);
  Win32Window::Point origin(10, 10);
  Win32Window::Size size(1280, 720);
  if (!window.Create(L"window_manager_example", origin, size)) {
    return EXIT_FAILURE;
  }
-  window.SetQuitOnClose(true);
+  window.SetQuitOnClose(false);
+
+  WindowManagerPlusPluginSetWindowCreatedCallback(
+      [](std::vector<std::string> command_line_arguments) {
+        flutter::DartProject project(L"data");
+
+        project.set_dart_entrypoint_arguments(
+            std::move(command_line_arguments));
+
+        auto window = std::make_shared<FlutterWindow>(project);
+        Win32Window::Point origin(10, 10);
+        Win32Window::Size size(1280, 720);
+        // Check whether window->Create or window->CreateAndShow is available.
+        // Take a look at the code above for the main flutter window and 
+        // what method the variable "FlutterWindow window(project)" calls
+        if (!window->Create(L"window_manager_example", origin, size)) {
+          std::cerr << "Failed to create a new window" << std::endl;
+        }
+        window->SetQuitOnClose(false);
+        return std::move(window);
+      });

  ::MSG msg;
  while (::GetMessage(&msg, nullptr, 0, 0)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }

  ::CoUninitialize();
  return EXIT_SUCCESS;
}
```

`window->SetQuitOnClose(false);` is necessary to prevent the application from closing when the window is closed.

If you want to close the App only when the main window is closed, you can set `window->SetQuitOnClose(true);` in the main window.
The others called inside the `WindowManagerPlusPluginSetWindowCreatedCallback` should be set to `false`.

### Usage

You must call `WindowManagerPlus.ensureInitialized` static method and `await` it before using any `WindowManagerPlus` methods or `WindowManagerPlus.current`.
It is used to initialize the plugin with the current window ID.

When creating a new window, the `args` parameter of the `main` function will have the window ID as a String.
You must parse it to an integer and pass it to the `WindowManagerPlus.ensureInitialized` method.
If the `args` parameter is empty or the first argument is not an integer, then we are in the main window, which ID is exactly `0`.

The other arguments will contain the arguments passed to the `WindowManagerPlus.createWindow` method, if any.

```dart
import 'package:flutter/material.dart';
import 'package:window_manager_plus_v2/window_manager_plus_v2.dart';

// Must add List<String> args parameter to your main function.
void main(List<String> args) async {
  WidgetsFlutterBinding.ensureInitialized();
  // await the initialization of the plugin.
  // Here is an example of how to use ensureInitialized in the main function:
  await WindowManagerPlus.ensureInitialized(args.isEmpty ? 0 : int.tryParse(args[0]) ?? 0);
  
  // Now you can use the plugin, such as WindowManagerPlus.current
  WindowOptions windowOptions = WindowOptions(
    size: Size(800, 600),
    center: true,
    backgroundColor: Colors.transparent,
    skipTaskbar: false,
    titleBarStyle: TitleBarStyle.hidden,
  );
  WindowManagerPlus.current.waitUntilReadyToShow(windowOptions, () async {
    await WindowManagerPlus.current.show();
    await WindowManagerPlus.current.focus();
  });

  runApp(MyApp());
}
```

#### Create a new window

You can create a new window by calling the `WindowManagerPlus.createWindow` static method.
If you want control the new window, you can use the return value of the method, which returns a `WindowManagerPlus` instance.

```dart
final newWindow = await WindowManagerPlus.createWindow(['my test arg 1', 'my test arg 2']);
if (newWindow != null) {
  print('New Created Window: $newWindow');
}
```

#### Communication between windows

You can communicate with another window by using the `WindowManagerPlus.invokeMethodToWindow` method.
The first parameter is the ID of the window you want to communicate with.
The second parameter is the method name you want to call.
The third parameter is the arguments you want to pass to the method, if any.

The other window must register and implement the `WindowListener` class and override the `WindowListener.onEventFromWindow` method to receive the event.

```dart
// assuming we are in the first window, Window ID 0, and we want to communicate with the second window.
final secondWindowId = 1; // ID of the second window
final result = await WindowManagerPlus.current.invokeMethodToWindow(secondWindowId, 'myTestMethod', ['arg1', 'arg2']);
// the result will be 'Hello from Window 1'

// assuming we are in the second window, Window ID 1
class _MyWidgetState extends State<MyWidget> with WindowListener {

  // ...

  @override
  void initState() {
    WindowManagerPlus.current.addListener(this);
    super.initState();
  }

  @override
  void dispose() {
    WindowManagerPlus.current.removeListener(this);
    super.dispose();
  }
  
  // ...
  
  @override
  Future<dynamic> onEventFromWindow(String eventName, int fromWindowId, dynamic arguments) async {
    print('[${WindowManagerPlus.current}] Event $eventName from Window $fromWindowId with arguments $arguments');
    return 'Hello from ${WindowManagerPlus.current}';
  }
}
```

Using `WindowManagerPlus.getAllWindowManagerIds()` static method you can get all the window manager ids available.

> Please see the example app of this plugin for a full example.

#### Listening events

The `WindowListener` mixin class is used to listen to window events.
If this is used as a Global Listener using the `WindowManagerPlus.addGlobalListener` static method,
the `windowId` parameter will be the ID of the window that emitted the event,
otherwise, it will be always `null`.

```dart
import 'package:flutter/cupertino.dart';
import 'package:window_manager_plus_v2/window_manager_plus_v2.dart';

class HomePage extends StatefulWidget {
  @override
  _HomePageState createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> with WindowListener {
  @override
  void initState() {
    super.initState();
    WindowManagerPlus.current.addListener(this);
  }

  @override
  void dispose() {
    WindowManagerPlus.current.removeListener(this);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    // ...
  }

  @override
  void onWindowEvent(String eventName, [int? windowId]) {
    print('[WindowManager] onWindowEvent: $eventName');
  }

  @override
  void onWindowClose([int? windowId]) {
    // do something
  }

  @override
  void onWindowFocus([int? windowId]) {
    // do something
  }

  @override
  void onWindowBlur([int? windowId]) {
    // do something
  }

  @override
  void onWindowMaximize([int? windowId]) {
    // do something
  }

  @override
  void onWindowUnmaximize([int? windowId]) {
    // do something
  }

  @override
  void onWindowMinimize([int? windowId]) {
    // do something
  }

  @override
  void onWindowRestore([int? windowId]) {
    // do something
  }

  @override
  void onWindowResize([int? windowId]) {
    // do something
  }

  @override
  void onWindowMove([int? windowId]) {
    // do something
  }

  @override
  void onWindowEnterFullScreen([int? windowId]) {
    // do something
  }

  @override
  void onWindowLeaveFullScreen([int? windowId]) {
    // do something
  }
}
```

#### Quit on close

If you need to use the hide method, you need to disable `QuitOnClose`.

##### macOS

Change the file `macos/Runner/AppDelegate.swift` as follows:

```diff
import Cocoa
import FlutterMacOS

@NSApplicationMain
class AppDelegate: FlutterAppDelegate {
  override func applicationShouldTerminateAfterLastWindowClosed(_ sender: NSApplication) -> Bool {
-    return true
+    return false
  }
}
```

##### Windows

Change the file `windows/runner/main.cpp` as follows:

```diff
#include <flutter/dart_project.h>
#include <flutter/flutter_view_controller.h>
#include <windows.h>

#include <iostream>

#include "flutter_window.h"
#include "utils.h"

int APIENTRY wWinMain(_In_ HINSTANCE instance,
                      _In_opt_ HINSTANCE prev,
                      _In_ wchar_t* command_line,
                      _In_ int show_command) {
  // Attach to console when present (e.g., 'flutter run') or create a
  // new console when running with a debugger.
  if (!::AttachConsole(ATTACH_PARENT_PROCESS) && ::IsDebuggerPresent()) {
    CreateAndAttachConsole();
  }

  // Initialize COM, so that it is available for use in the library and/or
  // plugins.
  ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);

  flutter::DartProject project(L"data");

  std::vector<std::string> command_line_arguments = GetCommandLineArguments();

  project.set_dart_entrypoint_arguments(std::move(command_line_arguments));

  FlutterWindow window(project);
  Win32Window::Point origin(10, 10);
  Win32Window::Size size(1280, 720);
  if (!window.CreateAndShow(L"window_manager_example", origin, size)) {
    return EXIT_FAILURE;
  }
-  window.SetQuitOnClose(true);
+  window.SetQuitOnClose(false);

  ::MSG msg;
  while (::GetMessage(&msg, nullptr, 0, 0)) {
    ::TranslateMessage(&msg);
    ::DispatchMessage(&msg);
  }

  ::CoUninitialize();
  return EXIT_SUCCESS;
}
```

#### Confirm before closing

```dart
import 'package:flutter/cupertino.dart';
import 'package:window_manager/window_manager.dart';

class HomePage extends StatefulWidget {
  @override
  _HomePageState createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> with WindowListener {
  @override
  void initState() {
    super.initState();
    WindowManagerPlus.current.addListener(this);
    _init();
  }

  @override
  void dispose() {
    WindowManagerPlus.current.removeListener(this);
    super.dispose();
  }

  void _init() async {
    // Add this line to override the default close handler
    await WindowManagerPlus.current.setPreventClose(true);
    setState(() {});
  }

  @override
  Widget build(BuildContext context) {
    // ...
  }

  @override
  void onWindowClose() async {
    bool _isPreventClose = await WindowManagerPlus.current.isPreventClose();
    if (_isPreventClose) {
      showDialog(
        context: context,
        builder: (_) {
          return AlertDialog(
            title: Text('Are you sure you want to close this window?'),
            actions: [
              TextButton(
                child: Text('No'),
                onPressed: () {
                  Navigator.of(context).pop();
                },
              ),
              TextButton(
                child: Text('Yes'),
                onPressed: () {
                  Navigator.of(context).pop();
                  await WindowManagerPlus.current.destroy();
                },
              ),
            ],
          );
        },
      );
    }
  }
}
```

#### Hidden at launch

##### Linux

Change the file `linux/my_application.cc` as follows:

```diff

...

// Implements GApplication::activate.
static void my_application_activate(GApplication* application) {
  
  ...

  gtk_window_set_default_size(window, 1280, 720);
-  gtk_widget_show(GTK_WIDGET(window));
+  gtk_widget_realize(GTK_WIDGET(window));

  g_autoptr(FlDartProject) project = fl_dart_project_new();
  fl_dart_project_set_dart_entrypoint_arguments(project, self->dart_entrypoint_arguments);

  FlView* view = fl_view_new(project);
  gtk_widget_show(GTK_WIDGET(view));
  gtk_container_add(GTK_CONTAINER(window), GTK_WIDGET(view));

  fl_register_plugins(FL_PLUGIN_REGISTRY(view));

  gtk_widget_grab_focus(GTK_WIDGET(view));
}

...

```

##### macOS

Change the file `macos/Runner/MainFlutterWindow.swift` as follows:

```diff
import Cocoa
import FlutterMacOS
+import window_manager

class MainFlutterWindow: NSWindow {
    override func awakeFromNib() {
        let flutterViewController = FlutterViewController.init()
        let windowFrame = self.frame
        self.contentViewController = flutterViewController
        self.setFrame(windowFrame, display: true)

        RegisterGeneratedPlugins(registry: flutterViewController)

        super.awakeFromNib()
    }

+    override public func order(_ place: NSWindow.OrderingMode, relativeTo otherWin: Int) {
+        super.order(place, relativeTo: otherWin)
+        hiddenWindowAtLaunch()
+    }
}

```

##### Windows

Change the file `windows/runner/win32_window.cpp` as follows:

```diff
bool Win32Window::CreateAndShow(const std::wstring& title,
                                const Point& origin,
                                const Size& size) {
  ...                              
  HWND window = CreateWindow(
-      window_class, title.c_str(), WS_OVERLAPPEDWINDOW | WS_VISIBLE,
+      window_class, title.c_str(),
+      WS_OVERLAPPEDWINDOW, // do not add WS_VISIBLE since the window will be shown later
      Scale(origin.x, scale_factor), Scale(origin.y, scale_factor),
      Scale(size.width, scale_factor), Scale(size.height, scale_factor),
      nullptr, nullptr, GetModuleHandle(nullptr), this);
```

Since flutter 3.7 new windows project
Change the file `windows/runner/flutter_window.cpp` as follows:

```diff
bool FlutterWindow::OnCreate() {
  ...
  flutter_controller_->engine()->SetNextFrameCallback([&]() {
-   this->Show();
+   //delete this->Show()
  });
```

Make sure to call `setState` once on the `onWindowFocus` event.

```dart
import 'package:flutter/cupertino.dart';
import 'package:window_manager/window_manager.dart';

class HomePage extends StatefulWidget {
  @override
  _HomePageState createState() => _HomePageState();
}

class _HomePageState extends State<HomePage> with WindowListener {
  @override
  void initState() {
    super.initState();
    WindowManagerPlus.current.addListener(this);
  }

  @override
  void dispose() {
    WindowManagerPlus.current.removeListener(this);
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    // ...
  }

  @override
  void onWindowFocus() {
    // Make sure to call once.
    setState(() {});
    // do something
  }
}

```

## Articles

- [Click the dock icon to restore after closing the window](https://leanflutter.dev/tips-and-tricks/002-click-dock-icon-to-restore-after-closing-the-window/)
- [Making the app single-instanced](https://leanflutter.dev/tips-and-tricks/001-making-the-app-single-instanced/)

## API

<!-- README_DOC_GEN -->
### WindowManagerPlus

#### Methods

##### [addListener](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/addListener.html)([WindowListener](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener-class.html) listener) → void

Add a listener to the window.

##### [blur](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/blur.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Removes focus from the window.

##### [center](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/center.html)({[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) animate = false}) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Moves window to the center of the screen.

##### [close](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/close.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Try to close the window.

##### [destroy](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/destroy.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Force closing the window.

##### [dock](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/dock.html)({required [DockSide](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/DockSide.html) side, required [int](https://api.flutter.dev/flutter/dart-core/int-class.html) width}) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Docks the window. only works on Windows

##### [focus](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/focus.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Focuses on the window.

##### [getBounds](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/getBounds.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[Rect](https://api.flutter.dev/flutter/dart-ui/Rect-class.html)\>

Returns `Rect` - The bounds of the window as Object.

##### [getDevicePixelRatio](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/getDevicePixelRatio.html)() → [double](https://api.flutter.dev/flutter/dart-core/double-class.html)

Get the device pixel ratio.

##### [getOpacity](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/getOpacity.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[double](https://api.flutter.dev/flutter/dart-core/double-class.html)\>

Returns `double` - between 0.0 (fully transparent) and 1.0 (fully opaque).

##### [getPosition](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/getPosition.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[Offset](https://api.flutter.dev/flutter/dart-ui/Offset-class.html)\>

Returns `Offset` - Contains the window's current position.

##### [getSize](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/getSize.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[Size](https://api.flutter.dev/flutter/dart-ui/Size-class.html)\>

Returns `Size` - Contains the window's width and height.

##### [getTitle](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/getTitle.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[String](https://api.flutter.dev/flutter/dart-core/String-class.html)\>

Returns `String` - The title of the native window.

##### [getTitleBarHeight](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/getTitleBarHeight.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[int](https://api.flutter.dev/flutter/dart-core/int-class.html)\>

Returns `int` - The title bar height of the native window.

##### [hasShadow](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/hasShadow.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window has a shadow. On Windows, always returns true unless window is frameless.

##### [hide](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/hide.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Hides the window.

##### [invokeMethodToWindow](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/invokeMethodToWindow.html)([int](https://api.flutter.dev/flutter/dart-core/int-class.html) targetWindowId, [String](https://api.flutter.dev/flutter/dart-core/String-class.html) method, \[dynamic args\]) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)

Invokes a method on the window with id `targetWindowId`. It could return a Future that resolves to the return value of the invoked method, otherwise `null`. Use [WindowListener.onEventFromWindow](https://pub.dev/documentation/window_manager_plus_v2/latest/WindowListener/onEventFromWindow.html) to listen for the event.

##### [isAlwaysOnBottom](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isAlwaysOnBottom.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window is always below other windows.

##### [isAlwaysOnTop](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isAlwaysOnTop.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window is always on top of other windows.

##### [isClosable](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isClosable.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window can be manually closed by user.

##### [isDockable](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isDockable.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window is dockable or not.

##### [isDocked](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isDocked.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[DockSide](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/DockSide.html)?\>

Returns `bool` - Whether the window is docked.

##### [isFocused](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isFocused.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether window is focused.

##### [isFullScreen](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isFullScreen.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window is in fullscreen mode.

##### [isMaximizable](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isMaximizable.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window can be manually maximized by the user.

##### [isMaximized](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isMaximized.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window is maximized.

##### [isMinimizable](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isMinimizable.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window can be manually minimized by the user.

##### [isMinimized](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isMinimized.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window is minimized.

##### [isMovable](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isMovable.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window can be moved by user.

##### [isPreventClose](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isPreventClose.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Check if is intercepting the native close signal.

##### [isResizable](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isResizable.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window can be manually resized by the user.

##### [isSkipTaskbar](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isSkipTaskbar.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether skipping taskbar is enabled.

##### [isVisible](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isVisible.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window is visible to the user.

##### [isVisibleOnAllWorkspaces](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/isVisibleOnAllWorkspaces.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Returns `bool` - Whether the window is visible on all workspaces.

##### [maximize](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/maximize.html)({[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) vertically = false}) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Maximizes the window. `vertically` simulates aero snap, only works on Windows

##### [minimize](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/minimize.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Minimizes the window. On some platforms the minimized window will be shown in the Dock.

##### [popUpWindowMenu](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/popUpWindowMenu.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

##### [removeListener](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/removeListener.html)([WindowListener](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener-class.html) listener) → void

Remove a listener from the window.

##### [restore](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/restore.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Restores the window from minimized state to its previous state.

##### [setAlignment](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setAlignment.html)([Alignment](https://api.flutter.dev/flutter/painting/Alignment-class.html) alignment, {[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) animate = false}) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Move the window to a position aligned with the screen.

##### [setAlwaysOnBottom](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setAlwaysOnBottom.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) isAlwaysOnBottom) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets whether the window should show always below other windows.

##### [setAlwaysOnTop](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setAlwaysOnTop.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) isAlwaysOnTop) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets whether the window should show always on top of other windows.

##### [setAsFrameless](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setAsFrameless.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

You can call this to remove the window frame (title bar, outline border, etc), which is basically everything except the Flutter view, also can call setTitleBarStyle(TitleBarStyle.normal) or setTitleBarStyle(TitleBarStyle.hidden) to restore it.

##### [setAspectRatio](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setAspectRatio.html)([double](https://api.flutter.dev/flutter/dart-core/double-class.html) aspectRatio) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

This will make a window maintain an aspect ratio.

##### [setBackgroundColor](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setBackgroundColor.html)([Color](https://api.flutter.dev/flutter/dart-ui/Color-class.html) backgroundColor) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets the background color of the window.

##### [setBadgeLabel](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setBadgeLabel.html)(\[[String](https://api.flutter.dev/flutter/dart-core/String-class.html)? label\]) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Set/unset label on taskbar(dock) app icon

##### [setBounds](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setBounds.html)([Rect](https://api.flutter.dev/flutter/dart-ui/Rect-class.html)? bounds, {[Offset](https://api.flutter.dev/flutter/dart-ui/Offset-class.html)? position, [Size](https://api.flutter.dev/flutter/dart-ui/Size-class.html)? size, [bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) animate = false}) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Resizes and moves the window to the supplied bounds.

##### [setBrightness](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setBrightness.html)([Brightness](https://api.flutter.dev/flutter/dart-ui/Brightness.html) brightness) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets the brightness of the window.

##### [setClosable](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setClosable.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) isClosable) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets whether the window can be manually closed by user.

##### [setFullScreen](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setFullScreen.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) isFullScreen) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets whether the window should be in fullscreen mode.

##### [setHasShadow](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setHasShadow.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) hasShadow) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets whether the window should have a shadow. On Windows, doesn't do anything unless window is frameless.

##### [setIcon](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setIcon.html)([String](https://api.flutter.dev/flutter/dart-core/String-class.html) iconPath) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets window/taskbar icon.

##### [setIgnoreMouseEvents](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setIgnoreMouseEvents.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) ignore, {[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) forward = false}) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Makes the window ignore all mouse events.

##### [setMaximizable](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setMaximizable.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) isMaximizable) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets whether the window can be manually maximized by the user.

##### [setMaximumSize](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setMaximumSize.html)([Size](https://api.flutter.dev/flutter/dart-ui/Size-class.html) size) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets the maximum size of window to `width` and `height`.

##### [setMinimizable](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setMinimizable.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) isMinimizable) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets whether the window can be manually minimized by user.

##### [setMinimumSize](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setMinimumSize.html)([Size](https://api.flutter.dev/flutter/dart-ui/Size-class.html) size) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets the minimum size of window to `width` and `height`.

##### [setMovable](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setMovable.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) isMovable) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets whether the window can be moved by user.

##### [setOpacity](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setOpacity.html)([double](https://api.flutter.dev/flutter/dart-core/double-class.html) opacity) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets the opacity of the window.

##### [setPosition](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setPosition.html)([Offset](https://api.flutter.dev/flutter/dart-ui/Offset-class.html) position, {[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) animate = false}) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Moves window to position.

##### [setPreventClose](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setPreventClose.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) isPreventClose) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Set if intercept the native close signal. May useful when combine with the onclose event listener. This will also prevent the manually triggered close event.

##### [setProgressBar](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setProgressBar.html)([double](https://api.flutter.dev/flutter/dart-core/double-class.html) progress) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets progress value in progress bar. Valid range is `0, 1.0`.

##### [setResizable](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setResizable.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) isResizable) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets whether the window can be manually resized by the user.

##### [setSize](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setSize.html)([Size](https://api.flutter.dev/flutter/dart-ui/Size-class.html) size, {[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) animate = false}) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Resizes the window to `width` and `height`.

##### [setSkipTaskbar](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setSkipTaskbar.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) isSkipTaskbar) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Makes the window not show in the taskbar / dock.

##### [setTitle](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setTitle.html)([String](https://api.flutter.dev/flutter/dart-core/String-class.html) title) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Changes the title of native window to title.

##### [setTitleBarStyle](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setTitleBarStyle.html)([TitleBarStyle](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/TitleBarStyle.html) titleBarStyle, {[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) windowButtonVisibility = true}) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Changes the title bar style of native window.

##### [setVisibleOnAllWorkspaces](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/setVisibleOnAllWorkspaces.html)([bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) visible, {[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)? visibleOnFullScreen}) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Sets whether the window should be visible on all workspaces.

##### [show](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/show.html)({[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html) inactive = false}) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Shows and gives focus to the window.

##### [startDragging](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/startDragging.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Starts a window drag based on the specified mouse-down event. On Windows, this is disabled during full screen mode.

##### [startResizing](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/startResizing.html)([ResizeEdge](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/ResizeEdge.html) resizeEdge) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Starts a window resize based on the specified mouse-down & mouse-move event. On Windows, this is disabled during full screen mode.

##### [toString](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/toString.html)() → [String](https://api.flutter.dev/flutter/dart-core/String-class.html)

A string representation of this object.

##### [undock](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/undock.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[bool](https://api.flutter.dev/flutter/dart-core/bool-class.html)\>

Undocks the window. only works on Windows

##### [unmaximize](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/unmaximize.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Unmaximizes the window.

##### [waitUntilReadyToShow](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/waitUntilReadyToShow.html)(\[[WindowOptions](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowOptions-class.html)? options, [VoidCallback](https://api.flutter.dev/flutter/dart-ui/VoidCallback.html)? callback\]) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Wait until ready to show.

#### Static Methods

##### [addGlobalListener](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/addGlobalListener.html)([WindowListener](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener-class.html) listener) → void

Add a global listener to the window.

##### [createWindow](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/createWindow.html)(\[[List](https://api.flutter.dev/flutter/dart-core/List-class.html)<[String](https://api.flutter.dev/flutter/dart-core/String-class.html)\>? args\]) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[WindowManagerPlus](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus-class.html)?\>

Create a new window.

##### [ensureInitialized](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/ensureInitialized.html)([int](https://api.flutter.dev/flutter/dart-core/int-class.html) windowId) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<void\>

Ensure the window manager for this `windowId` is initialized. Must be called before accessing the [WindowManagerPlus.current](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/current.html).

##### [fromWindowId](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/fromWindowId.html)([int](https://api.flutter.dev/flutter/dart-core/int-class.html) windowId) → [WindowManagerPlus](https://pub.dev/documentation/window_manager_plus_v2/latest/WindowManagerPlus-class.html)

Get the window manager from the window id.

##### [getAllWindowManagerIds](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/getAllWindowManagerIds.html)() → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)<[List](https://api.flutter.dev/flutter/dart-core/List-class.html)<[int](https://api.flutter.dev/flutter/dart-core/int-class.html)\>\>

Get all window manager ids.

##### [removeGlobalListener](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowManagerPlus/removeGlobalListener.html)([WindowListener](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener-class.html) listener) → void

Remove a global listener from the window.

### WindowListener

#### Methods

##### [onEventFromWindow](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onEventFromWindow.html)([String](https://api.flutter.dev/flutter/dart-core/String-class.html) eventName, [int](https://api.flutter.dev/flutter/dart-core/int-class.html) fromWindowId, dynamic arguments) → [Future](https://api.flutter.dev/flutter/dart-async/Future-class.html)

Event from other windows.

##### [onWindowBlur](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowBlur.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted when the window loses focus.

##### [onWindowClose](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowClose.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted when the window is going to be closed.

##### [onWindowDocked](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowDocked.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted when the window entered a docked state.

##### [onWindowEnterFullScreen](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowEnterFullScreen.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted when the window enters a full-screen state.

##### [onWindowEvent](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowEvent.html)([String](https://api.flutter.dev/flutter/dart-core/String-class.html) eventName, \[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted all events.

##### [onWindowFocus](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowFocus.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted when the window gains focus.

##### [onWindowLeaveFullScreen](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowLeaveFullScreen.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted when the window leaves a full-screen state.

##### [onWindowMaximize](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowMaximize.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted when window is maximized.

##### [onWindowMinimize](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowMinimize.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted when the window is minimized.

##### [onWindowMove](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowMove.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted when the window is being moved to a new position.

##### [onWindowMoved](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowMoved.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted once when the window is moved to a new position.

##### [onWindowResize](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowResize.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted after the window has been resized.

##### [onWindowResized](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowResized.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted once when the window has finished being resized.

##### [onWindowRestore](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowRestore.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted when the window is restored from a minimized state.

##### [onWindowUndocked](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowUndocked.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted when the window leaves a docked state.

##### [onWindowUnmaximize](https://pub.dev/documentation/window_manager_plus_v2/latest/window_manager_plus_v2/WindowListener/onWindowUnmaximize.html)(\[[int](https://api.flutter.dev/flutter/dart-core/int-class.html)? windowId\]) → void

Emitted when the window exits from a maximized state.

<!-- README_DOC_GEN -->

## Contributors

<!-- ALL-CONTRIBUTORS-LIST:START - Do not remove or modify this section -->
<!-- prettier-ignore-start -->
<!-- markdownlint-disable -->

<!-- markdownlint-restore -->
<!-- prettier-ignore-end -->

<!-- ALL-CONTRIBUTORS-LIST:END -->

## License

[MIT](./LICENSE)
