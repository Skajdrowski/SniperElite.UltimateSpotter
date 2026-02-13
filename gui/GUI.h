#pragma once
#include "../dxsdk/include/d3d8.h"
#include "../dllmain.h"

static std::wstring Trim(const std::wstring& text);

class GUI
{
public:
    // Initializes the GUI subsystem with the host application's device
    static void Start(LPDIRECT3DDEVICE8 device);

    // Invoked every frame (from EndScene hooks) to draw the GUI
    static void Render();
    static void RenderGreeting();

    // Call this to clean up when your DLL is detaching
    static void Shutdown();

    // Notify GUI about device loss/reset events so resources can be recreated
    static void OnDeviceLost();
    static void OnDeviceReset(LPDIRECT3DDEVICE8 device);

    // Toggles the GUI's visibility
    static void Toggle();
    static void ToogleGreeting();

    // Returns true if the GUI is currently visible
    static bool isVisible;
    static bool isGreeting;

    // Returns true when the GUI has been initialized with a valid device
    static bool isInitialized;

    // Retrieves the current mouse cursor position mapped into the Direct3D viewport space
    static bool GetCursorPosition(POINT& cursorViewport);

    static void DrawGuiContent(const RECT& viewport, bool hasCursorPosition, const POINT& cursorPosition,
        bool leftMouseDown, bool mousePressedThisFrame);
    static void DrawGreetingContent();

    static void SnapshotGuiState(bool hasCursor, const POINT& cursorPosition, bool leftMouseDown);
    static void SnapshotGreetingState();

    static bool ShouldRedrawGui(bool hasCursorPosition, const POINT& cursorPosition, bool leftMouseDown, bool textChanged);
    static bool ShouldRedrawGreeting();
};
