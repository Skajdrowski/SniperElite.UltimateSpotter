#pragma once
#include "../dxsdk/include/d3d8.h"
#include "../dllmain.h"

struct PromptState
{
    std::wstring input;
    std::wstring savedValue;
    std::wstring savedInput;
};

enum class TextFieldFocus
{
    None,
    FetchInput,
    SavedValue
};

struct FetchState
{
    bool hasResult = false;
    uint32_t uid = 0;
	void* player = nullptr;
    std::wstring statusMessage;
    std::wstring displayName;
    bool online = false;
    std::string ip;
};

struct SnapshotState
{
    bool hasCursor = false;
    POINT cursor = {};
    bool leftMouseDown = false;
    bool isVisible = false;
    TextFieldFocus textField = TextFieldFocus::None;
    int currentPage = 0;
    size_t playerSignature = 0;
};

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
