#pragma warning(disable:4996)

#include "GUI.h"
#include "RenderTarget.h"
#include "RenderManager.h"

#include <algorithm>
#include <array>
#include <functional>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

bool GUI::isVisible = false;
bool GUI::isGreeting = true;
bool GUI::isInitialized = false;

int g_playerListScroll = 0;
bool g_playerListDraggingScroll = false;
int g_playerListDragMouseOffsetY = 0;

LPDIRECT3DDEVICE8 g_pd3dDevice = nullptr;

// State backup
DWORD g_dwOldVertexShader = 0;
DWORD g_dwOldStateBlock = 0;

bool g_isMouseCaptured = false;
bool g_isCursorVisible = false;
HCURSOR g_arrowCursor = nullptr;

RECT g_lastClipRect = {};
bool g_lastClipRectValid = false;

DWORD g_lastGuiRedrawTick = 0;

RenderTarget* g_guiRenderTarget = nullptr;
RenderTarget* g_greetingRenderTarget = nullptr;
bool g_guiDirty = true;
bool g_greetingDirty = true;

D3DVIEWPORT8 g_lastViewport = {};
bool g_lastViewportValid = false;

PromptState g_prompt;

constexpr size_t kPromptMaxChars = 48;
TextFieldFocus g_activeTextField = TextFieldFocus::None;

bool g_leftMouseWasDown = false;
std::array<bool, 256> g_keyLatch{};

FetchState g_fetch;

int g_currentPage = 0;

static bool g_loadoutPresetDropdownOpen = false;

SnapshotState g_snapshot;

std::wstring g_snapshotGreetingText;
bool g_snapshotGreetingVisible = false;

bool IsGameLoading()
{
    const uint8_t* const loadPtr = reinterpret_cast<const uint8_t*>(LoadingFlagAddr);
    return *loadPtr == 0;
}

void DestroyRenderTargets()
{
    delete g_guiRenderTarget;
    g_guiRenderTarget = nullptr;
    delete g_greetingRenderTarget;
    g_greetingRenderTarget = nullptr;
    g_guiDirty = true;
    g_greetingDirty = true;
}

void CreateStateBlock()
{
    if (!g_pd3dDevice)
        return;

    if (g_dwOldStateBlock)
    {
        g_pd3dDevice->DeleteStateBlock(g_dwOldStateBlock);
        g_dwOldStateBlock = 0;
    }

    if (FAILED(g_pd3dDevice->CreateStateBlock(D3DSBT_ALL, &g_dwOldStateBlock)))
        g_dwOldStateBlock = 0;
}

void EnsureRenderTargets(const D3DVIEWPORT8& viewport)
{
    if (!g_pd3dDevice)
        return;

    const int width = static_cast<int>(viewport.Width);
    const int height = static_cast<int>(viewport.Height);
    if (width <= 0 || height <= 0)
        return;

    const bool viewportChanged = !g_lastViewportValid
        || g_lastViewport.Width != viewport.Width
        || g_lastViewport.Height != viewport.Height;

    if (!g_guiRenderTarget || viewportChanged)
    {
        delete g_guiRenderTarget;
        g_guiRenderTarget = nullptr;

        g_guiRenderTarget = new (std::nothrow) RenderTarget(g_pd3dDevice, width, height);
        if (g_guiRenderTarget && !g_guiRenderTarget->IsValid())
        {
            delete g_guiRenderTarget;
            g_guiRenderTarget = nullptr;
        }

        g_guiDirty = true;
    }

    if (!g_greetingRenderTarget || viewportChanged)
    {
        delete g_greetingRenderTarget;
        g_greetingRenderTarget = nullptr;

        g_greetingRenderTarget = new (std::nothrow) RenderTarget(g_pd3dDevice, width, height);
        if (g_greetingRenderTarget && !g_greetingRenderTarget->IsValid())
        {
            delete g_greetingRenderTarget;
            g_greetingRenderTarget = nullptr;
        }

        g_greetingDirty = true;
    }

    g_lastViewport = viewport;
    g_lastViewportValid = true;
}

RECT GetViewportScreenRect(HWND focusWindow)
{
    RECT viewport = Render::GetViewport(g_pd3dDevice);
    POINT topLeft{ viewport.left, viewport.top };
    POINT bottomRight{ viewport.right, viewport.bottom };
    ClientToScreen(focusWindow, &topLeft);
    ClientToScreen(focusWindow, &bottomRight);
    RECT screenRect{ topLeft.x, topLeft.y, bottomRight.x, bottomRight.y };
    return screenRect;
}

void UpdateMouseCaptureState(bool shouldCapture)
{
    if (!shouldCapture || !g_pd3dDevice)
    {
        if (g_isMouseCaptured)
        {
            ClipCursor(nullptr);
            ReleaseCapture();
            g_isMouseCaptured = false;
            g_lastClipRectValid = false;
        }
        return;
    }

    D3DDEVICE_CREATION_PARAMETERS params = {};
    if (FAILED(g_pd3dDevice->GetCreationParameters(&params)) || !params.hFocusWindow)
    {
        if (g_isMouseCaptured)
        {
            ClipCursor(nullptr);
            ReleaseCapture();
            g_isMouseCaptured = false;
            g_lastClipRectValid = false;
        }
        return;
    }

    const RECT clipRect = GetViewportScreenRect(params.hFocusWindow);

    const bool clipChanged = !g_lastClipRectValid
        || g_lastClipRect.left != clipRect.left
        || g_lastClipRect.top != clipRect.top
        || g_lastClipRect.right != clipRect.right
        || g_lastClipRect.bottom != clipRect.bottom;

    if (clipChanged)
    {
        ClipCursor(&clipRect);
        g_lastClipRect = clipRect;
        g_lastClipRectValid = true;
    }

    if (!g_isMouseCaptured)
    {
        SetCapture(params.hFocusWindow);
        g_isMouseCaptured = true;
    }
}

void UpdateCursorVisibility(bool shouldShow)
{
    if (!isHost)
        return;

    if (g_isCursorVisible == shouldShow)
        return;

    if (shouldShow)
    {
        if (!g_arrowCursor)
            g_arrowCursor = LoadCursor(nullptr, IDC_ARROW);
        if (g_arrowCursor)
            SetCursor(g_arrowCursor);
        while (ShowCursor(TRUE) < 0) {}
    }
    else
    {
        while (ShowCursor(FALSE) >= 0) {}
    }

    g_isCursorVisible = shouldShow;
}

void SetTextFieldFocus(TextFieldFocus newFocus)
{
    if (g_activeTextField == newFocus)
        return;

    g_activeTextField = newFocus;
    g_keyLatch.fill(false);

    g_guiDirty = true;

    if (newFocus == TextFieldFocus::SavedValue && g_prompt.savedInput.empty())
        g_prompt.savedInput = g_prompt.savedValue;
}

bool IsPointInsideRect(const POINT& pt, const RECT& rect)
{
    return pt.x >= rect.left && pt.x <= rect.right && pt.y >= rect.top && pt.y <= rect.bottom;
}

void ShrinkRectToTextWidth(RECT& rect, LPD3DXFONT font, const char* text, int paddingRight)
{
    if (!text || !font)
        return;

    RECT textSize = Render::GetTextSize(font, text);
    const int textW = (std::max)(0, static_cast<int>(textSize.right - textSize.left));
    const int desiredRight = rect.left + textW + paddingRight;
    if (desiredRight < rect.right)
        rect.right = desiredRight;
}

bool ProcessPromptInput()
{
    if (g_activeTextField == TextFieldFocus::None)
        return false;

    std::wstring* activeBuffer = nullptr;
    if (g_activeTextField == TextFieldFocus::FetchInput)
        activeBuffer = &g_prompt.input;
    else if (g_activeTextField == TextFieldFocus::SavedValue)
        activeBuffer = &g_prompt.savedInput;

    if (!activeBuffer)
        return false;

    bool changed = false;
    const bool shiftHeld = (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0;

    auto handleCharacterKey = [&](int vk, wchar_t baseChar, wchar_t shiftedChar = 0)
        {
            const bool isDown = (GetAsyncKeyState(vk) & 0x8000) != 0;
            if (isDown && !g_keyLatch[vk])
            {
                wchar_t ch = baseChar;
                if (shiftHeld && shiftedChar)
                    ch = shiftedChar;

                if (activeBuffer->size() < kPromptMaxChars && ch)
                {
                    activeBuffer->push_back(ch);
                    changed = true;
                }

                g_keyLatch[vk] = true;
            }
            else if (!isDown)
            {
                g_keyLatch[vk] = false;
            }
        };

    auto handleActionKey = [&](int vk, const std::function<void()>& action)
        {
            const bool isDown = (GetAsyncKeyState(vk) & 0x8000) != 0;
            if (isDown && !g_keyLatch[vk])
            {
                action();
                g_keyLatch[vk] = true;
                changed = true;
            }
            else if (!isDown)
            {
                g_keyLatch[vk] = false;
            }
        };

    for (int vk = 'A'; vk <= 'Z'; ++vk)
    {
        const wchar_t lower = static_cast<wchar_t>(vk + ('a' - 'A'));
        const wchar_t upper = static_cast<wchar_t>(vk);
        handleCharacterKey(vk, lower, upper);
    }

    for (int vk = '0'; vk <= '9'; ++vk)
    {
        const wchar_t digit = static_cast<wchar_t>(vk);
        handleCharacterKey(vk, digit, digit);
    }

    handleCharacterKey(VK_SPACE, L' ', L' ');
    handleCharacterKey(VK_OEM_MINUS, L'-', L'_');
    handleCharacterKey(VK_OEM_PERIOD, L'.', L':');

    handleActionKey(VK_BACK, [&]()
        {
            if (!activeBuffer->empty())
            {
                activeBuffer->pop_back();
                changed = true;
            }
        });

    handleActionKey(VK_RETURN, [&]()
        {
            if (g_activeTextField == TextFieldFocus::FetchInput)
            {
                g_prompt.savedValue = g_prompt.input;
                g_prompt.savedInput = g_prompt.savedValue;
            }
            else if (g_activeTextField == TextFieldFocus::SavedValue)
            {
                const std::wstring trimmed = Trim(g_prompt.savedInput);
                g_prompt.savedValue = trimmed;
                g_prompt.savedInput = trimmed;
            }
        });

    return changed;
}

std::wstring Trim(const std::wstring& text)
{
    const size_t begin = text.find_first_not_of(L" \t\r\n");
    if (begin == std::wstring::npos)
        return L"";
    const size_t end = text.find_last_not_of(L" \t\r\n");
    return text.substr(begin, end - begin + 1);
}

std::string FormatPointer(void* ptr)
{
    std::stringstream ss;
    ss << "0x" << std::uppercase << std::hex << reinterpret_cast<uintptr_t>(ptr);
    return ss.str();
}

std::wstring GetActivePromptText()
{
    if (!g_prompt.input.empty())
        return g_prompt.input;
    return g_prompt.savedValue;
}

bool TryParseUID(const std::wstring& text, uint32_t& outUid)
{
    const std::wstring trimmed = Trim(text);
    if (trimmed.empty())
        return false;

    wchar_t* endPtr = nullptr;
    unsigned long value = std::wcstoul(trimmed.c_str(), &endPtr, 0);
    if (endPtr == trimmed.c_str() || *endPtr != L'\0')
        return false;

    outUid = static_cast<uint32_t>(value);
    return true;
}

void PerformInventoryFetch()
{
    g_fetch.hasResult = false;
    g_fetch.uid = 0;
    g_fetch.player = nullptr;
    g_fetch.displayName.clear();
    g_fetch.online = false;
    g_fetch.ip.clear();
    g_fetch.statusMessage.clear();
    g_guiDirty = true;

    const std::wstring inputText = GetActivePromptText();
    const std::wstring trimmedInput = Trim(inputText);
    if (trimmedInput.empty())
    {
        g_fetch.statusMessage = L"Choose player's nickname before fetching.";
        g_guiDirty = true;
        return;
    }

    const auto resolveByName = [&](const std::wstring& name) -> bool
        {
            if (name.empty())
                return false;

            const std::map<std::wstring, PlayerFetchEntry>::iterator dirIt = g_playerDirectory.find(name);
            if (dirIt == g_playerDirectory.end())
                return false;

            const PlayerFetchEntry& entry = dirIt->second;
            if (entry.uid == 0)
                return false;

            g_fetch.uid = entry.uid;
            g_fetch.player = entry.player;
            if (!g_fetch.player)
            {
                const std::map<uint32_t, void*>::iterator invIt = uIdToPlayer.find(entry.uid);
                if (invIt != uIdToPlayer.end())
                    g_fetch.player = invIt->second;
            }

            g_fetch.displayName = !entry.displayName.empty() ? entry.displayName : name;
            g_fetch.online = entry.isOnline;
            if (!entry.ipAddress.empty())
                g_fetch.ip = entry.ipAddress;

            g_fetch.hasResult = true;
            g_guiDirty = true;
            return true;
        };

    if (resolveByName(trimmedInput))
        return;

    uint32_t parsedUID = 0;
    if (!TryParseUID(trimmedInput, parsedUID))
    {
        g_fetch.statusMessage = L"Player not found.";
        g_guiDirty = true;
        return;
    }

    g_fetch.uid = parsedUID;
    g_fetch.displayName = trimmedInput;

    const std::map<uint32_t, void*>::iterator it = uIdToPlayer.find(parsedUID);
    if (it != uIdToPlayer.end())
    {
        g_fetch.player = it->second;
        g_fetch.online = true;
    }
    else
    {
        g_fetch.player = nullptr;
        g_fetch.online = false;
    }

    for (const auto& [name, entry] : g_playerDirectory)
    {
        if (entry.uid == parsedUID)
        {
            if (g_fetch.displayName.empty())
                g_fetch.displayName = !entry.displayName.empty() ? entry.displayName : name;
            g_fetch.online = entry.isOnline;
            if (!entry.ipAddress.empty())
                g_fetch.ip = entry.ipAddress;

            break;
        }
    }

    g_fetch.hasResult = true;
    g_guiDirty = true;
}

size_t ComputePlayerDirectorySignature()
{
    size_t signature = g_playerDirectory.size();
    std::hash<std::wstring> wstringHash;
    for (const auto& [name, entry] : g_playerDirectory)
    {
        size_t part = wstringHash(name);
        part ^= static_cast<size_t>(entry.uid) + 0x9e3779b9 + (part << 6) + (part >> 2);
        part ^= static_cast<size_t>(entry.isOnline);
        signature ^= part + 0x9e3779b9 + (signature << 6) + (signature >> 2);
    }
    return signature;
}

void ApplyGuiRenderState()
{
    g_pd3dDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, TRUE);
    g_pd3dDevice->SetRenderState(D3DRS_SRCBLEND, D3DBLEND_SRCALPHA);
    g_pd3dDevice->SetRenderState(D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA);
    g_pd3dDevice->SetRenderState(D3DRS_ZENABLE, D3DZB_FALSE);
    g_pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);
    g_pd3dDevice->SetRenderState(D3DRS_CULLMODE, D3DCULL_NONE);
    g_pd3dDevice->SetTexture(0, nullptr);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_DIFFUSE);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1);
    g_pd3dDevice->SetTextureStageState(0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE);
    g_pd3dDevice->SetTextureStageState(1, D3DTSS_COLOROP, D3DTOP_DISABLE);
    g_pd3dDevice->SetTextureStageState(1, D3DTSS_ALPHAOP, D3DTOP_DISABLE);
}

void GUI::SnapshotGuiState(bool hasCursor, const POINT& cursorPosition, bool leftMouseDown)
{
    g_snapshot.hasCursor = hasCursor;
    g_snapshot.cursor = cursorPosition;
    g_snapshot.leftMouseDown = leftMouseDown;
    g_snapshot.isVisible = isVisible;
    g_snapshot.textField = g_activeTextField;
    g_snapshot.currentPage = g_currentPage;
    g_snapshot.playerSignature = ComputePlayerDirectorySignature();
}

void GUI::SnapshotGreetingState()
{
    g_snapshotGreetingText = greetBuffer;
    g_snapshotGreetingVisible = isGreeting;
}

void GUI::DrawGuiContent(const RECT& viewport, bool hasCursorPosition, const POINT& cursorPosition,
    bool leftMouseDown, bool mousePressedThisFrame)
{
    if (!isHost)
        return;

    bool promptVisibleThisFrame = false;

    const char* checkMark = "X";

    if (isVisible)
    {
        const int screenWidth = viewport.right - viewport.left;
        const int screenHeight = viewport.bottom - viewport.top;

        if (screenWidth > 0 && screenHeight > 0)
        {
            const int minDimension = (screenWidth < screenHeight) ? screenWidth : screenHeight;
            int panelSize = minDimension > 0 ? minDimension : 16;
            if (panelSize < 16) panelSize = 16;

            const int panelX = viewport.left + (screenWidth - panelSize) / 2;
            const int panelY = viewport.top + (screenHeight - panelSize) / 2;

            Render::DrawSquare(g_pd3dDevice, panelX, panelY, panelSize, 0xC8000000);
            UpdateMouseCaptureState(false);

            // =====================================================
            // TOOLBAR (pages)
            // =====================================================
            const int pageCount = 2;
            const int toolbarH = panelSize / 10;
            const int buttonW = panelSize / pageCount;

            Render::Draw(g_pd3dDevice, panelX, panelY, panelSize, toolbarH, 0xAA000000);

            for (int i = 0; i < pageCount; ++i)
            {
                RECT btn{
                    panelX + i * buttonW,
                    panelY,
                    panelX + (i + 1) * buttonW,
                    panelY + toolbarH
                };

                const bool hovered = hasCursorPosition && IsPointInsideRect(cursorPosition, btn);
                if (mousePressedThisFrame && hovered)
                {
                    g_currentPage = i;
                    g_guiDirty = true;
                }

                Render::Draw(
                    g_pd3dDevice,
                    btn.left, btn.top,
                    buttonW, toolbarH,
                    i == g_currentPage ? 0xFF2F4F8F : (hovered ? 0xFF1E1E1E : 0xFF101010)
                );

                Render::Outline(g_pd3dDevice, btn.left, btn.top, buttonW, toolbarH, 0xFFFF0000);

                static char label[24];
                if (i == 0)
                    sprintf(label, "LOBBY MANAGMENT");
                if (i == 1)
                    sprintf(label, "PLAYER INVENTORIES");

                Render::Fonts::Tabs->DrawTextA(label, -1, &btn, DT_VCENTER | DT_CENTER | DT_NOCLIP, 0xFFFFFFFF);
            }

            const int versionPad = 2;
            RECT versionRect{
                panelX + versionPad,
                panelY + versionPad,
                panelX + panelSize - versionPad,
                panelY + panelSize - versionPad
            };
            Render::Fonts::MenuBold->DrawTextA("Ultimate Spotter v1.2", -1, &versionRect, DT_RIGHT | DT_BOTTOM | DT_NOCLIP, 0xFF037d50);

            // =====================================================
            // CONTENT AREA
            // =====================================================
            const int padX = 20;
            int y = panelY + toolbarH + 20;

            // =====================================================
            // LOBBY MANAGEMENT PAGE
            // =====================================================
            if (g_currentPage == 0)
            {
                if (g_activeTextField != TextFieldFocus::None)
                    ProcessPromptInput();

                // Player list
                const int listHeight = 360;
                const int listHeaderH = 22;
                const int listRowH = 24;
                RECT listRect{
                    panelX + padX,
                    y,
                    panelX + panelSize - padX,
                    y + listHeight
                };

                Render::Draw(g_pd3dDevice,
                    listRect.left,
                    listRect.top,
                    listRect.right - listRect.left,
                    listRect.bottom - listRect.top,
                    0xAA0F0F0F);
                Render::Outline(g_pd3dDevice,
                    listRect.left,
                    listRect.top,
                    listRect.right - listRect.left,
                    listRect.bottom - listRect.top,
                    0xFF000000);

                Render::Text(Render::Fonts::MenuBold,
                    listRect.left + 6,
                    listRect.top + 2,
                    0xFFFFFFFF,
                    "Player listing");

                std::vector<std::pair<std::wstring, const PlayerFetchEntry*>> playerRows;
                playerRows.reserve(g_playerDirectory.size());
                for (const auto& [name, entry] : g_playerDirectory)
                    playerRows.emplace_back(name, &entry);

                std::sort(playerRows.begin(), playerRows.end(),
                    [](const auto& a, const auto& b)
                    {
                        if (a.second->uid != b.second->uid)
                            return a.second->uid < b.second->uid;
                        return a.first < b.first;
                    });

                const int usableHeight = listHeight - listHeaderH - 6;
                const int maxRows = usableHeight / listRowH;
                const int virtualSlots = 50;
                const int totalRows = virtualSlots;
                const int maxScroll = (maxRows > 0) ? (std::max)(0, totalRows - maxRows) : 0;

                if (g_playerListScroll > maxScroll)
                    g_playerListScroll = maxScroll;
                if (g_playerListScroll < 0)
                    g_playerListScroll = 0;

                RECT listContentRect{
                    listRect.left,
                    listRect.top + listHeaderH,
                    listRect.right,
                    listRect.bottom
                };

                const bool overList = hasCursorPosition && IsPointInsideRect(cursorPosition, listContentRect);

                if (mousePressedThisFrame && !leftMouseDown)
                    g_playerListDraggingScroll = false;

                const int wheelDelta = static_cast<int>(GetAsyncKeyState(VK_MBUTTON));
                (void)wheelDelta;

                if (overList)
                {
                    if ((GetAsyncKeyState(VK_UP) & 0x8000) != 0)
                        g_playerListScroll -= 1;
                    if ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0)
                        g_playerListScroll += 1;
                    if ((GetAsyncKeyState(VK_PRIOR) & 0x8000) != 0)
                        g_playerListScroll -= maxRows;
                    if ((GetAsyncKeyState(VK_NEXT) & 0x8000) != 0)
                        g_playerListScroll += maxRows;

                    if (g_playerListScroll > maxScroll)
                        g_playerListScroll = maxScroll;
                    if (g_playerListScroll < 0)
                        g_playerListScroll = 0;
                }

                const int scrollbarW = 10;
                const int scrollbarPad = 4;
                RECT scrollbarRect{
                    listRect.right - scrollbarPad - scrollbarW,
                    listRect.top + listHeaderH + 2,
                    listRect.right - scrollbarPad,
                    listRect.bottom - 4
                };

                const int rowWidth = (listRect.right - listRect.left) - 12 - (scrollbarW + scrollbarPad);

                const int trackH = (std::max)(1, static_cast<int>(scrollbarRect.bottom - scrollbarRect.top));
                const float visibleFrac = (totalRows > 0) ? (static_cast<float>(maxRows) / static_cast<float>(totalRows)) : 1.0f;
                int thumbH = static_cast<int>(trackH * (std::min)(1.0f, (std::max)(0.05f, visibleFrac)));
                if (thumbH < 12) thumbH = 12;
                if (thumbH > trackH) thumbH = trackH;

                const int thumbRange = (std::max)(0, trackH - thumbH);
                int thumbY = scrollbarRect.top;
                if (maxScroll > 0)
                    thumbY += static_cast<int>((static_cast<float>(g_playerListScroll) / static_cast<float>(maxScroll)) * thumbRange);

                RECT thumbRect{
                    scrollbarRect.left,
                    thumbY,
                    scrollbarRect.right,
                    thumbY + thumbH
                };

                const bool overThumb = hasCursorPosition && IsPointInsideRect(cursorPosition, thumbRect);
                const bool overScrollbar = hasCursorPosition && IsPointInsideRect(cursorPosition, scrollbarRect);

                if (mousePressedThisFrame && overThumb)
                {
                    g_playerListDraggingScroll = true;
                    g_playerListDragMouseOffsetY = cursorPosition.y - thumbRect.top;
                }

                if (!leftMouseDown)
                    g_playerListDraggingScroll = false;

                if (g_playerListDraggingScroll && hasCursorPosition)
                {
                    int desiredThumbTop = cursorPosition.y - g_playerListDragMouseOffsetY;
                    if (desiredThumbTop < scrollbarRect.top)
                        desiredThumbTop = scrollbarRect.top;
                    if (desiredThumbTop > scrollbarRect.bottom - thumbH)
                        desiredThumbTop = scrollbarRect.bottom - thumbH;

                    const int thumbPos = desiredThumbTop - scrollbarRect.top;
                    if (thumbRange > 0 && maxScroll > 0)
                    {
                        const float t = static_cast<float>(thumbPos) / static_cast<float>(thumbRange);
                        g_playerListScroll = static_cast<int>(t * maxScroll + 0.5f);
                    }
                    else
                    {
                        g_playerListScroll = 0;
                    }

                    if (g_playerListScroll > maxScroll)
                        g_playerListScroll = maxScroll;
                    if (g_playerListScroll < 0)
                        g_playerListScroll = 0;
                }
                else if (mousePressedThisFrame && overScrollbar && !overThumb)
                {
                    if (hasCursorPosition)
                    {
                        if (cursorPosition.y < thumbRect.top)
                            g_playerListScroll -= maxRows;
                        else if (cursorPosition.y > thumbRect.bottom)
                            g_playerListScroll += maxRows;

                        if (g_playerListScroll > maxScroll)
                            g_playerListScroll = maxScroll;
                        if (g_playerListScroll < 0)
                            g_playerListScroll = 0;
                    }
                }

                Render::Draw(g_pd3dDevice,
                    scrollbarRect.left,
                    scrollbarRect.top,
                    scrollbarRect.right - scrollbarRect.left,
                    scrollbarRect.bottom - scrollbarRect.top,
                    0xFF0E0E0E);
                Render::Outline(g_pd3dDevice,
                    scrollbarRect.left,
                    scrollbarRect.top,
                    scrollbarRect.right - scrollbarRect.left,
                    scrollbarRect.bottom - scrollbarRect.top,
                    0xFF000000);

                Render::Draw(g_pd3dDevice,
                    thumbRect.left,
                    thumbRect.top,
                    thumbRect.right - thumbRect.left,
                    thumbRect.bottom - thumbRect.top,
                    g_playerListDraggingScroll ? 0xFF2F4F8F : (overThumb ? 0xFF3A3A3A : 0xFF2A2A2A));
                Render::Outline(g_pd3dDevice,
                    thumbRect.left,
                    thumbRect.top,
                    thumbRect.right - thumbRect.left,
                    thumbRect.bottom - thumbRect.top,
                    0xFF000000);

                const int totalPlayers = static_cast<int>(playerRows.size());
                for (int localIdx = 0; localIdx < maxRows; ++localIdx)
                {
                    const int globalIdx = g_playerListScroll + localIdx;
                    if (globalIdx < 0 || globalIdx >= totalRows)
                        continue;
                    if (globalIdx >= totalPlayers)
                        continue;

                    const int rowY = listRect.top + listHeaderH + localIdx * listRowH;
                    RECT rowRect{
                        listRect.left + 4,
                        rowY + 2,
                        listRect.left + 4 + rowWidth,
                        rowY + listRowH
                    };

                    const bool hovered = hasCursorPosition && IsPointInsideRect(cursorPosition, rowRect);
                    const PlayerFetchEntry* entryPtr = playerRows[globalIdx].second;
                    const bool isOnline = entryPtr && entryPtr->isOnline;

                    const D3DCOLOR rowColor = hovered
                        ? 0xFF2F4F8F
                        : (isOnline ? 0xFF1A2A1A : 0xFF1E1E1E);

                    Render::Draw(g_pd3dDevice,
                        rowRect.left,
                        rowRect.top,
                        rowRect.right - rowRect.left,
                        rowRect.bottom - rowRect.top,
                        rowColor);

                    std::wstringstream rowText;
                    rowText << playerRows[globalIdx].first;
                    if (entryPtr && entryPtr->uid == 1000)
                        rowText << " [Host]";
                    else
                        rowText << (isOnline ? " [Online]" : " [Offline]");

                    Render::TextW(Render::Fonts::MenuText,
                        rowRect.left + 6,
                        rowRect.top + 3,
                        0xFF90EE90,
                        rowText.str().c_str());

                    if (mousePressedThisFrame && hovered && entryPtr)
                    {
                        const std::wstring& selectedName = entryPtr->displayName.empty()
                            ? playerRows[globalIdx].first
                            : entryPtr->displayName;
                        g_prompt.input = selectedName;
                        g_prompt.savedValue = selectedName;
                        g_prompt.savedInput = selectedName;
                        SetTextFieldFocus(TextFieldFocus::None);
                        PerformInventoryFetch();
                    }
                }

                y += listHeight + 20;

                const int infoLabelX = panelX + padX;
                const int infoValueX = infoLabelX + 180;

                Render::Text(Render::Fonts::MenuText, infoLabelX, y, 0xFFFFFFFF, "Player name:");
                const std::wstring nameValue = g_fetch.hasResult && !g_fetch.displayName.empty() ? g_fetch.displayName : L"-";
                Render::TextW(Render::Fonts::MenuText, infoValueX, y, 0xFF90EE90, nameValue.c_str());
                y += 18;

                Render::Text(Render::Fonts::MenuText, infoLabelX, y, 0xFFFFFFFF, "Player address:");
                std::string addressValue = "-";
                if (g_fetch.hasResult && !g_fetch.ip.empty())
                    addressValue = g_fetch.ip;
                else
                    addressValue = "-";
                Render::Text(Render::Fonts::MenuText, infoValueX, y, 0xFF90EE90, addressValue.c_str());
                y += 24;

                if (!g_fetch.statusMessage.empty())
                {
                    Render::TextW(Render::Fonts::MenuText, infoLabelX, y, 0xFFAAAAAA, g_fetch.statusMessage.c_str());
                    y += 20;
                }

                // KICK
                const std::wstring success = L" [Success]";
                const std::wstring fail = L" [Failed]";
                const std::wstring fetchFirst = L" Firstly choose a player from 'Player listing' before issuing anything.";

                const int dangerW = 64;
                const int dangerH = 32;

                RECT kickRect{
                    panelX + padX,
                    y,
                    panelX + padX + dangerW,
                    y + dangerH
                };

                const bool overKick = hasCursorPosition && IsPointInsideRect(cursorPosition, kickRect);
                const bool kickPressed = mousePressedThisFrame && overKick;
                const bool canKick = g_fetch.hasResult && g_fetch.uid != 0;

                Render::Draw(g_pd3dDevice,
                    kickRect.left, kickRect.top,
                    dangerW, dangerH,
                    overKick ? 0xFFFFA500 : 0xFFFF8300);

                Render::Outline(g_pd3dDevice,
                    kickRect.left, kickRect.top,
                    dangerW, dangerH,
                    0xFFFFFFFF);

                Render::Fonts::MenuBold->DrawTextA("KICK", -1, &kickRect, DT_VCENTER | DT_CENTER | DT_NOCLIP, 0xFFFFFFFF);

                if (kickPressed)
                {
                    std::wstringstream status;
                    const PlayerFetchEntry* entry = nullptr;
                    for (const auto& [string, value] : g_playerDirectory)
                    {
                        if (value.uid == g_fetch.uid)
                        {
                            entry = &value;
                            break;
                        }
                    }
                    if (canKick)
                    {
                        const uint8_t result = kick(g_fetch.uid, 4);

                        if (!g_fetch.displayName.empty())
                            status << L" (" << g_fetch.displayName << L")";

                        status << (result ? success : fail);
                        if (!result && entry->isOnline)
                            status << L" You're likely trying to kick yourself or an internal error occured.";
                        else if (!result && !entry->isOnline)
                            status << L" You tried to kick an offline player.";

                        g_fetch.statusMessage = status.str();
                        g_guiDirty = true;
                    }
                    else
                    {
                        status << fetchFirst;
                        g_fetch.statusMessage = status.str();
                        g_guiDirty = true;
                    }
                }

                // BAN
                RECT banRect{
                    kickRect.right + 12,
                    y,
                    kickRect.right + 12 + dangerW,
                    y + dangerH
                };

                const bool overBan = hasCursorPosition && IsPointInsideRect(cursorPosition, banRect);
                const bool banPressed = mousePressedThisFrame && overBan;
                const bool canBan = g_fetch.hasResult && g_fetch.uid != 0 && !g_fetch.ip.empty();

                Render::Draw(g_pd3dDevice,
                    banRect.left, banRect.top,
                    dangerW, dangerH,
                    overBan ? 0xFFB00000 : 0xFF8B0000);

                Render::Outline(g_pd3dDevice,
                    banRect.left, banRect.top,
                    dangerW, dangerH,
                    0xFFFFFFFF);

                Render::Fonts::MenuBold->DrawTextA("BAN", -1, &banRect, DT_VCENTER | DT_CENTER | DT_NOCLIP, 0xFFFFFFFF);

                if (banPressed)
                {
                    std::wstringstream status;
                    const PlayerFetchEntry* entry = nullptr;
                    for (const auto& [string, value] : g_playerDirectory)
                    {
                        if (value.uid == g_fetch.uid)
                        {
                            entry = &value;
                            break;
                        }
                    }
                    if (canBan)
                    {
                        const bool newlyBanned = banIpAddress(g_fetch.ip);
                        const uint8_t result = kick(g_fetch.uid, 4);

                        if (!g_fetch.displayName.empty())
                            status << L" (" << g_fetch.displayName << L")";

                        status << (newlyBanned && result ? success : fail);
                        if (!newlyBanned && entry->isOnline)
                            status << L" You're likely trying to ban yourself or you already banned that IP.";
                        else if (!newlyBanned)
                            status << L" This IP is likely already banned.";

                        g_fetch.statusMessage = status.str();
                        g_guiDirty = true;
                    }
                    else
                    {
                        status << fetchFirst;
                        g_fetch.statusMessage = status.str();
                        g_guiDirty = true;
                    }
                }

                // UNBAN
                RECT unbanRect{
                    banRect.right + 12,
                    y,
                    banRect.right + 12 + dangerW,
                    y + dangerH
                };

                const bool overUnban = hasCursorPosition && IsPointInsideRect(cursorPosition, unbanRect);
                const bool unbanPressed = mousePressedThisFrame && overUnban;
                const bool canUnban = g_fetch.hasResult && g_fetch.uid != 0 && !g_fetch.ip.empty();

                Render::Draw(g_pd3dDevice,
                    unbanRect.left, unbanRect.top,
                    dangerW, dangerH,
                    overUnban ? 0xFFA857EB : 0xFF65358C);

                Render::Outline(g_pd3dDevice,
                    unbanRect.left, unbanRect.top,
                    dangerW, dangerH,
                    0xFFFFFFFF);

                Render::Fonts::MenuBold->DrawTextA("UNBAN", -1, &unbanRect, DT_VCENTER | DT_CENTER | DT_NOCLIP, 0xFFFFFFFF);

                if (unbanPressed)
                {
                    std::wstringstream status;
                    const PlayerFetchEntry* entry = nullptr;
                    for (const auto& [string, value] : g_playerDirectory)
                    {
                        if (value.uid == g_fetch.uid)
                        {
                            entry = &value;
                            break;
                        }
                    }
                    if (canUnban)
                    {
                        const bool removedBan = unBanIpAddress(g_fetch.ip);

                        if (!g_fetch.displayName.empty())
                            status << L" (" << g_fetch.displayName << L")";

                        status << (removedBan ? success : fail);
                        if (!removedBan)
                            status << L" The player you're trying to unban wasn't banned or an internal error occured.";
                        else if (removedBan)
                            status << L" Selected player should now be able to rejoin your lobbies.";

                        g_fetch.statusMessage = status.str();
                        g_guiDirty = true;
                    }
                    else
                    {
                        status << fetchFirst;
                        g_fetch.statusMessage = status.str();
                        g_guiDirty = true;
                    }
                }
                y += dangerH + 12;

                //Autobalance checkmark
                const int checkboxSize = 18;
                const int checkboxLabelOffset = 28;

                RECT checkboxRect{
                    panelX + padX,
                    y,
                    panelX + padX + checkboxSize,
                    y + checkboxSize
                };

                const char* autoBalanceLabel = "Disable TDM AutoBalance";
                RECT checkboxLabelRect{
                    checkboxRect.right + 8,
                    y,
                    panelX + panelSize - padX,
                    y + checkboxSize
                };
                ShrinkRectToTextWidth(checkboxLabelRect, Render::Fonts::MenuText, autoBalanceLabel, 4);

                const bool overCheckbox =
                    hasCursorPosition &&
                    (IsPointInsideRect(cursorPosition, checkboxRect) ||
                        IsPointInsideRect(cursorPosition, checkboxLabelRect));

                if (mousePressedThisFrame && overCheckbox)
                {
                    autoBalance = !autoBalance;
                    WritePrivateProfileStringW(L"LOBBY", L"AutoBalance", autoBalance ? L"1" : L"0", iniPath);
                    g_guiDirty = true;
                }

                Render::Draw(
                    g_pd3dDevice,
                    checkboxRect.left,
                    checkboxRect.top,
                    checkboxSize,
                    checkboxSize,
                    overCheckbox ? 0xFF2F4F8F : 0xFF1E1E1E
                );
                Render::Outline(
                    g_pd3dDevice,
                    checkboxRect.left,
                    checkboxRect.top,
                    checkboxSize,
                    checkboxSize,
                    0xFFFFFFFF
                );
                if (autoBalance)
                    Render::Fonts::MenuTabs->DrawTextA(checkMark, -1, &checkboxRect,
                        DT_VCENTER | DT_CENTER | DT_NOCLIP, 0xFF7CFC00);

                Render::Text(
                    Render::Fonts::MenuText,
                    checkboxLabelRect.left,
                    checkboxLabelRect.top + 2,
                    0xFFFFFFFF,
                    autoBalanceLabel
                );

                // Enable 19in1 checkmark
                RECT enableMapsCheckboxRect{
                    panelX + panelSize / 2,
                    y,
                    panelX + panelSize / 2 + checkboxSize,
                    y + checkboxSize
                };

                const char* enableMapsLabel = "Enable 19in1 maps";
                RECT enableMapsCheckboxLabelRect{
                    enableMapsCheckboxRect.right + 8,
                    y,
                    panelX + panelSize - padX,
                    y + checkboxSize
                };
                ShrinkRectToTextWidth(enableMapsCheckboxLabelRect, Render::Fonts::MenuText, enableMapsLabel, 4);

                const bool overEnableMapsCheckbox =
                    hasCursorPosition &&
                    (IsPointInsideRect(cursorPosition, enableMapsCheckboxRect) ||
                        IsPointInsideRect(cursorPosition, enableMapsCheckboxLabelRect));

                if (mousePressedThisFrame && overEnableMapsCheckbox)
                {
                    unlockMaps = !unlockMaps;
                    listMaps();
                    g_guiDirty = true;
                }

                Render::Draw(
                    g_pd3dDevice,
                    enableMapsCheckboxRect.left,
                    enableMapsCheckboxRect.top,
                    checkboxSize,
                    checkboxSize,
                    overEnableMapsCheckbox ? 0xFF2F4F8F : 0xFF1E1E1E
                );
                Render::Outline(
                    g_pd3dDevice,
                    enableMapsCheckboxRect.left,
                    enableMapsCheckboxRect.top,
                    checkboxSize,
                    checkboxSize,
                    0xFFFFFFFF
                );
                if (unlockMaps)
                    Render::Fonts::MenuTabs->DrawTextA(checkMark, -1, &enableMapsCheckboxRect,
                        DT_VCENTER | DT_CENTER | DT_NOCLIP, 0xFF7CFC00);

                Render::Text(
                    Render::Fonts::MenuText,
                    enableMapsCheckboxLabelRect.left,
                    enableMapsCheckboxLabelRect.top + 2,
                    0xFFFFFFFF,
                    enableMapsLabel
                );

                if (overEnableMapsCheckbox)
                {
                    Render::Text(
                        Render::Fonts::MenuText,
                        enableMapsCheckboxLabelRect.left,
                        enableMapsCheckboxLabelRect.top - 2 - checkboxSize,
                        0xFFAAAAAA,
                        "Toggle this only if you have 19in1 installed!"
                    );
                }

                y += checkboxSize + 8;

                // Anti OOB checkmark
                RECT antiCheckboxRect{
                    panelX + padX,
                    y,
                    panelX + padX + checkboxSize,
                    y + checkboxSize
                };

                const char* antiOobLabel = "Anti OOB";
                RECT antiCheckboxLabelRect{
                    antiCheckboxRect.right + 8,
                    y,
                    panelX + panelSize / 2 - 10,
                    y + checkboxSize
                };
                ShrinkRectToTextWidth(antiCheckboxLabelRect, Render::Fonts::MenuText, antiOobLabel, 4);

                const bool overAntiCheckbox =
                    hasCursorPosition &&
                    (IsPointInsideRect(cursorPosition, antiCheckboxRect) ||
                        IsPointInsideRect(cursorPosition, antiCheckboxLabelRect));

                if (mousePressedThisFrame && overAntiCheckbox)
                {
                    antiOOB = !antiOOB;
                    WritePrivateProfileStringW(L"LOBBY", L"AntiOOB", antiOOB ? L"1" : L"0", iniPath);
                    g_guiDirty = true;
                }

                Render::Draw(
                    g_pd3dDevice,
                    antiCheckboxRect.left,
                    antiCheckboxRect.top,
                    checkboxSize,
                    checkboxSize,
                    overAntiCheckbox ? 0xFF2F4F8F : 0xFF1E1E1E
                );
                Render::Outline(
                    g_pd3dDevice,
                    antiCheckboxRect.left,
                    antiCheckboxRect.top,
                    checkboxSize,
                    checkboxSize,
                    0xFFFFFFFF
                );
                if (antiOOB)
                    Render::Fonts::MenuTabs->DrawTextA(checkMark, -1, &antiCheckboxRect,
                        DT_VCENTER | DT_CENTER | DT_NOCLIP, 0xFF7CFC00);

                Render::Text(
                    Render::Fonts::MenuText,
                    antiCheckboxLabelRect.left,
                    antiCheckboxLabelRect.top + 2,
                    0xFFFFFFFF,
                    antiOobLabel
                );

                if (overAntiCheckbox)
                {
                    Render::Text(
                        Render::Fonts::MenuText,
                        antiCheckboxLabelRect.left,
                        antiCheckboxLabelRect.top + 2 + checkboxSize,
                        0xFFAAAAAA,
                        "Prevents players from entering inaccessible areas"
                    );
                }

                // Custom spawns checkmark
                RECT customSpawnsCheckboxRect{
                    panelX + panelSize / 2,
                    y,
                    panelX + panelSize / 2 + checkboxSize,
                    y + checkboxSize
                };

                const char* customSpawnsLabel = "Custom spawns";
                RECT customSpawnsCheckboxLabelRect{
                    customSpawnsCheckboxRect.right + 8,
                    y,
                    panelX + panelSize - padX,
                    y + checkboxSize
                };
                ShrinkRectToTextWidth(customSpawnsCheckboxLabelRect, Render::Fonts::MenuText, customSpawnsLabel, 4);

                const bool overCustomSpawnsCheckbox =
                    hasCursorPosition &&
                    (IsPointInsideRect(cursorPosition, customSpawnsCheckboxRect) ||
                        IsPointInsideRect(cursorPosition, customSpawnsCheckboxLabelRect));
                const bool customSpawnsAvailable = strcmp(curLevel, "ntend.pc") == 0;

                if (mousePressedThisFrame && overCustomSpawnsCheckbox && customSpawnsAvailable)
                {
                    customSpawns = !customSpawns;
                    WritePrivateProfileStringW(L"LOBBY", L"CustomSpawns", customSpawns ? L"1" : L"0", iniPath);
                    g_guiDirty = true;
                }

                Render::Draw(
                    g_pd3dDevice,
                    customSpawnsCheckboxRect.left,
                    customSpawnsCheckboxRect.top,
                    checkboxSize,
                    checkboxSize,
                    !customSpawnsAvailable ? 0xFF141414 : (overCustomSpawnsCheckbox ? 0xFF2F4F8F : 0xFF1E1E1E)
                );
                Render::Outline(
                    g_pd3dDevice,
                    customSpawnsCheckboxRect.left,
                    customSpawnsCheckboxRect.top,
                    checkboxSize,
                    checkboxSize,
                    !customSpawnsAvailable ? 0xFF666666 : 0xFFFFFFFF
                );
                if (customSpawns)
                    Render::Fonts::MenuTabs->DrawTextA(checkMark, -1, &customSpawnsCheckboxRect,
                        DT_VCENTER | DT_CENTER | DT_NOCLIP, !customSpawnsAvailable ? 0xFF6B6B6B : 0xFF7CFC00);

                Render::Text(
                    Render::Fonts::MenuText,
                    customSpawnsCheckboxLabelRect.left,
                    customSpawnsCheckboxLabelRect.top + 2,
                    !customSpawnsAvailable ? 0xFF8A8A8A : 0xFFFFFFFF,
                    customSpawnsLabel
                );

                if (overCustomSpawnsCheckbox && !customSpawnsAvailable)
                {
                    Render::Text(
                        Render::Fonts::MenuText,
                        customSpawnsCheckboxLabelRect.left,
                        customSpawnsCheckboxLabelRect.top + 2 + checkboxSize,
                        0xFFAAAAAA,
                        "You can't toggle custom spawns while in a lobby"
                    );
                }

                // Bottom value + Save
                const int bottomH = 40;
                const int saveW = 120;

                RECT valueRect{
                    panelX + padX,
                    panelY + panelSize - bottomH - 20,
                    panelX + panelSize - saveW - padX - 10,
                    panelY + panelSize - 20
                };

                RECT saveRect{
                    valueRect.right + 10,
                    valueRect.top,
                    valueRect.right + 10 + saveW,
                    valueRect.bottom
                };

                const bool overValue = hasCursorPosition && IsPointInsideRect(cursorPosition, valueRect);
                const bool overSave = hasCursorPosition && IsPointInsideRect(cursorPosition, saveRect);

                if (mousePressedThisFrame)
                {
                    if (overValue)
                    {
                        SetTextFieldFocus(TextFieldFocus::SavedValue);
                    }
                    else if (overSave)
                    {
                        const std::wstring trimmed = Trim(g_prompt.savedInput);
                        g_prompt.savedValue = trimmed;
                        g_prompt.savedInput = trimmed;
                        g_guiDirty = true;

                        OpenClipboard(nullptr);
                        EmptyClipboard();
                        size_t sizeInBytes = (g_prompt.savedInput.size() + 1) * sizeof(wchar_t);

                        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, sizeInBytes);
                        memcpy(GlobalLock(hMem), g_prompt.savedInput.c_str(), sizeInBytes);
                        GlobalUnlock(hMem);

                        SetClipboardData(CF_UNICODETEXT, hMem);
                        CloseClipboard();

                        SetTextFieldFocus(TextFieldFocus::None);
                    }
                    else
                    {
                        SetTextFieldFocus(TextFieldFocus::None);
                    }
                }

                const bool savedValueFocused = g_activeTextField == TextFieldFocus::SavedValue;

                Render::Draw(g_pd3dDevice,
                    valueRect.left, valueRect.top,
                    valueRect.right - valueRect.left,
                    bottomH,
                    savedValueFocused ? 0xFF1A1A2E : 0xFF111111);

                Render::Outline(g_pd3dDevice,
                    valueRect.left, valueRect.top,
                    valueRect.right - valueRect.left,
                    bottomH,
                    savedValueFocused ? 0xFF6AA4FF : 0xFF888888);

                const bool showValuePlaceholder = g_prompt.savedInput.empty() && !savedValueFocused;
                const std::wstring valueText = showValuePlaceholder ? L"Type value to save..." : g_prompt.savedInput;

                Render::TextW(Render::Fonts::MenuText,
                    valueRect.left + 6,
                    valueRect.top + 11,
                    showValuePlaceholder ? 0x80FFFFFF : 0xFFFFFFFF,
                    valueText.c_str());

                Render::Draw(g_pd3dDevice,
                    saveRect.left, saveRect.top,
                    saveW, bottomH,
                    overSave ? 0xFF3E6ACB : 0xFF2F4F8F);

                Render::Outline(g_pd3dDevice,
                    saveRect.left, saveRect.top,
                    saveW, bottomH,
                    0xFFFFFFFF);

                Render::Fonts::MenuBold->DrawTextA("Copy to Clipboard", -1, &saveRect,
                    DT_VCENTER | DT_CENTER | DT_NOCLIP, 0xFFFFFFFF);
            }

            // =====================================================
            // PLAYER INVENTORIES PAGE
            // =====================================================
            if (g_currentPage == 1)
            {
                const int controlW = panelSize - (padX * 2);
                const int btnH = 32;

                RECT toggleRect{
                    panelX + padX,
                    y,
                    panelX + padX + controlW,
                    y + btnH
                };

                const bool overToggle = hasCursorPosition && IsPointInsideRect(cursorPosition, toggleRect);
                if (mousePressedThisFrame && overToggle)
                {
                    g_loadoutPresetsEnabled = !g_loadoutPresetsEnabled;
                    if (!g_loadoutPresetsEnabled)
                        g_loadoutPresetDropdownOpen = false;
                    g_guiDirty = true;
                }

                Render::Draw(g_pd3dDevice,
                    toggleRect.left,
                    toggleRect.top,
                    toggleRect.right - toggleRect.left,
                    btnH,
                    g_loadoutPresetsEnabled
                    ? (overToggle ? 0xFF18E314 : 0xFF2CCC29)
                    : (overToggle ? 0xFF1E1E1E : 0xFF101010));

                Render::Outline(g_pd3dDevice,
                    toggleRect.left,
                    toggleRect.top,
                    toggleRect.right - toggleRect.left,
                    btnH,
                    0xFFFFFFFF);

                Render::Text(Render::Fonts::MenuBold,
                    toggleRect.left + 12,
                    toggleRect.top + 7,
                    0xFFFFFFFF,
                    g_loadoutPresetsEnabled ? "Custom Loadouts: ON" : "Custom Loadouts: OFF");

                y += btnH + 12;

                static const std::array<const char*, 5> kLoadoutPresets{
                    "Sniper only",
                    "Machine Gun only",
                    "Pistol only",
                    "Grenades only",
                    "No explosives"
                };

                if (g_selectedLoadoutPresetIndex < 0)
                    g_selectedLoadoutPresetIndex = 0;
                if (g_selectedLoadoutPresetIndex >= kLoadoutPresets.size())
                    g_selectedLoadoutPresetIndex = kLoadoutPresets.size() - 1;

                RECT comboRect{
                    panelX + padX,
                    y,
                    panelX + padX + controlW,
                    y + btnH
                };

                const bool overCombo = hasCursorPosition && IsPointInsideRect(cursorPosition, comboRect);
                const bool comboPressed = mousePressedThisFrame && overCombo;

                if (comboPressed && g_loadoutPresetsEnabled)
                {
                    g_loadoutPresetDropdownOpen = !g_loadoutPresetDropdownOpen;
                    g_guiDirty = true;
                }

                Render::Draw(g_pd3dDevice,
                    comboRect.left,
                    comboRect.top,
                    comboRect.right - comboRect.left,
                    btnH,
                    g_loadoutPresetsEnabled
                    ? (overCombo ? 0xFF1A1A2E : 0xFF111111)
                    : 0xFF0C0C0C);

                Render::Outline(g_pd3dDevice,
                    comboRect.left,
                    comboRect.top,
                    comboRect.right - comboRect.left,
                    btnH,
                    g_loadoutPresetsEnabled ? 0xFF888888 : 0xFF444444);

                Render::Text(Render::Fonts::MenuText,
                    comboRect.left + 10,
                    comboRect.top + 8,
                    g_loadoutPresetsEnabled ? 0xFFFFFFFF : 0xFF888888,
                    kLoadoutPresets[g_selectedLoadoutPresetIndex]);

                Render::Text(Render::Fonts::MenuText,
                    comboRect.right - 18,
                    comboRect.top + 8,
                    g_loadoutPresetsEnabled ? 0xFFFFFFFF : 0xFF888888,
                    g_loadoutPresetDropdownOpen ? "^" : "v");

                y += btnH;

                if (g_loadoutPresetsEnabled && g_loadoutPresetDropdownOpen)
                {
                    const int itemH = 26;
                    const int listH = itemH * static_cast<int>(kLoadoutPresets.size());
                    RECT listRect{
                        comboRect.left,
                        comboRect.bottom,
                        comboRect.right,
                        comboRect.bottom + listH
                    };

                    Render::Draw(g_pd3dDevice,
                        listRect.left,
                        listRect.top,
                        listRect.right - listRect.left,
                        listRect.bottom - listRect.top,
                        0xFF0F0F0F);
                    Render::Outline(g_pd3dDevice,
                        listRect.left,
                        listRect.top,
                        listRect.right - listRect.left,
                        listRect.bottom - listRect.top,
                        0xFFFFFFFF);

                    bool clickedAnyItem = false;
                    for (int i = 0; i < static_cast<int>(kLoadoutPresets.size()); ++i)
                    {
                        RECT itemRect{
                            listRect.left + 2,
                            listRect.top + (i * itemH),
                            listRect.right - 2,
                            listRect.top + ((i + 1) * itemH)
                        };

                        const bool overItem = hasCursorPosition && IsPointInsideRect(cursorPosition, itemRect);
                        const bool itemPressed = mousePressedThisFrame && overItem;

                        const D3DCOLOR bg = (i == g_selectedLoadoutPresetIndex)
                            ? 0xFF2F4F8F
                            : (overItem ? 0xFF1E1E1E : 0xFF101010);

                        Render::Draw(g_pd3dDevice,
                            itemRect.left,
                            itemRect.top,
                            itemRect.right - itemRect.left,
                            itemRect.bottom - itemRect.top,
                            bg);

                        Render::Text(Render::Fonts::MenuText,
                            itemRect.left + 10,
                            itemRect.top + 5,
                            0xFFFFFFFF,
                            kLoadoutPresets[i]);

                        if (itemPressed)
                        {
                            g_selectedLoadoutPresetIndex = i;
                            WritePrivateProfileStringW(L"INVENTORIES", L"LoadoutPreset", std::to_wstring(g_selectedLoadoutPresetIndex).c_str(), iniPath);
                            g_loadoutPresetDropdownOpen = false;
                            g_guiDirty = true;
                            clickedAnyItem = true;
                        }
                    }

                    const bool overList = hasCursorPosition && IsPointInsideRect(cursorPosition, listRect);
                    if (mousePressedThisFrame && !overList && !overCombo && !overToggle && !clickedAnyItem)
                    {
                        g_loadoutPresetDropdownOpen = false;
                        g_guiDirty = true;
                    }

                    y = listRect.bottom + 12;
                }
                else
                {
                    if (mousePressedThisFrame && !overCombo && !overToggle)
                    {
                        if (g_loadoutPresetDropdownOpen)
                        {
                            g_loadoutPresetDropdownOpen = false;
                            g_guiDirty = true;
                        }
                    }

                    y += 12;
                }
                y += 4;

                // Knife checkmark
                const int checkboxSize = 18;
                const int checkboxLabelOffset = 28;

                RECT knifeCheckboxRect{
                    panelX + padX,
                    y,
                    panelX + padX + checkboxSize,
                    y + checkboxSize
                };

                const char* knifeLabel = "Everyone has knife";
                RECT knifeCheckboxLabelRect{
                    knifeCheckboxRect.right + 8,
                    y,
                    panelX + panelSize - padX,
                    y + checkboxSize
                };
                ShrinkRectToTextWidth(knifeCheckboxLabelRect, Render::Fonts::MenuText, knifeLabel, 4);

                const bool overCheckbox =
                    hasCursorPosition &&
                    (IsPointInsideRect(cursorPosition, knifeCheckboxRect) ||
                        IsPointInsideRect(cursorPosition, knifeCheckboxLabelRect));

                if (mousePressedThisFrame && overCheckbox)
                {
                    g_everyoneHasKnife = !g_everyoneHasKnife;
                    WritePrivateProfileStringW(L"INVENTORIES", L"EveryOneKnife", g_everyoneHasKnife ? L"1" : L"0", iniPath);
                    g_guiDirty = true;
                }

                Render::Draw(
                    g_pd3dDevice,
                    knifeCheckboxRect.left,
                    knifeCheckboxRect.top,
                    checkboxSize,
                    checkboxSize,
                    overCheckbox ? 0xFF2F4F8F : 0xFF1E1E1E
                );
                Render::Outline(
                    g_pd3dDevice,
                    knifeCheckboxRect.left,
                    knifeCheckboxRect.top,
                    checkboxSize,
                    checkboxSize,
                    0xFFFFFFFF
                );
                if (g_everyoneHasKnife)
                    Render::Fonts::MenuTabs->DrawTextA(checkMark, -1, &knifeCheckboxRect,
                        DT_VCENTER | DT_CENTER | DT_NOCLIP, 0xFF7CFC00);

                Render::Text(
                    Render::Fonts::MenuText,
                    knifeCheckboxLabelRect.left,
                    knifeCheckboxLabelRect.top + 2,
                    0xFFFFFFFF,
                    knifeLabel
                );
            }

            promptVisibleThisFrame = true;
        }
    }
    else
    {
        SetTextFieldFocus(TextFieldFocus::None);
    }

    if (!promptVisibleThisFrame)
        SetTextFieldFocus(TextFieldFocus::None);
}

void GUI::DrawGreetingContent()
{
    if (!isHost)
        return;

    const D3DCOLOR greetColor = isPopulated ? 0xFFFFFF00 : 0xFFFFFFFF;
    Render::TextWOutlined(Render::Fonts::Menu, 30, 35, greetColor, greetBuffer, 0xFF000000);
    if (isPopulated)
        Render::TextOutlined(Render::Fonts::MenuText, 30, 60, 0xFFFFFFFF,
            "To enable menu, hit INSERT\nTo disable this fancy text, hit HOME", 0xFF000000);
}

bool GUI::ShouldRedrawGui(bool hasCursorPosition, const POINT& cursorPosition, bool leftMouseDown, bool textChanged)
{
    if (g_guiDirty)
        return true;

    if (g_snapshot.hasCursor != hasCursorPosition)
        return true;

    if (hasCursorPosition && (g_snapshot.cursor.x != cursorPosition.x || g_snapshot.cursor.y != cursorPosition.y))
    {
        const DWORD now = GetTickCount();
        if (now - g_lastGuiRedrawTick >= 33)
            return true;
    }

    if (g_snapshot.leftMouseDown != leftMouseDown)
        return true;

    if (textChanged)
        return true;

    if (g_snapshot.isVisible != isVisible)
        return true;

    if (g_snapshot.textField != g_activeTextField
        || g_snapshot.currentPage != g_currentPage)
        return true;

    if (g_snapshot.playerSignature != ComputePlayerDirectorySignature())
        return true;

    return false;
}

bool GUI::ShouldRedrawGreeting()
{
    if (g_greetingDirty)
        return true;

    if (g_snapshotGreetingVisible != isGreeting)
        return true;

    if (g_snapshotGreetingText != greetBuffer)
        return true;

    return false;
}

bool GUI::GetCursorPosition(POINT& cursorViewport)
{
    if (!g_pd3dDevice)
        return false;

    POINT cursorScreen;
    if (!GetCursorPos(&cursorScreen))
        return false;

    D3DDEVICE_CREATION_PARAMETERS params = {};
    if (FAILED(g_pd3dDevice->GetCreationParameters(&params)) || !params.hFocusWindow)
        return false;

    if (!ScreenToClient(params.hFocusWindow, &cursorScreen))
        return false;

    const RECT viewport = Render::GetViewport(g_pd3dDevice);
    cursorViewport.x = viewport.left + cursorScreen.x;
    cursorViewport.y = viewport.top + cursorScreen.y;
    return true;
}

void GUI::Start(LPDIRECT3DDEVICE8 device)
{
    if (!device || isInitialized)
        return;

    g_pd3dDevice = device;

    CreateStateBlock();
    Render::Initialise(g_pd3dDevice);
    isInitialized = true;
    g_guiDirty = true;
    g_greetingDirty = true;
}

void GUI::Render()
{
    if (!isInitialized || !isVisible || !g_pd3dDevice)
    {
        UpdateCursorVisibility(false);
        UpdateMouseCaptureState(false);
        return;
    }

    if (IsGameLoading())
    {
        UpdateCursorVisibility(false);
        UpdateMouseCaptureState(false);
        return;
    }

    if (!g_dwOldStateBlock)
        CreateStateBlock();

    if (!g_dwOldStateBlock)
    {
        UpdateCursorVisibility(false);
        UpdateMouseCaptureState(false);
        return;
    }

    D3DVIEWPORT8 viewport = {};
    g_pd3dDevice->GetViewport(&viewport);
    if (viewport.Width == 0 || viewport.Height == 0)
        return;

    EnsureRenderTargets(viewport);
    if (!g_guiRenderTarget)
        return;

    POINT cursorPosition = {};
    const bool hasCursorPosition = GetCursorPosition(cursorPosition);
    const bool leftMouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
    const bool mousePressedThisFrame = leftMouseDown && !g_leftMouseWasDown;

    bool textChanged = false;
    if (isVisible && g_currentPage == 0 && g_activeTextField != TextFieldFocus::None)
        textChanged = ProcessPromptInput();

    const bool shouldRedraw = ShouldRedrawGui(hasCursorPosition, cursorPosition, leftMouseDown, textChanged);
    UpdateCursorVisibility(isVisible);

    g_pd3dDevice->CaptureStateBlock(g_dwOldStateBlock);
    g_pd3dDevice->GetVertexShader(&g_dwOldVertexShader);

    ApplyGuiRenderState();

    if (shouldRedraw)
    {
        g_lastGuiRedrawTick = GetTickCount();
        D3DVIEWPORT8 oldViewport = viewport;
        g_guiRenderTarget->BeginScene();
        DrawGuiContent(Render::GetViewport(g_pd3dDevice), hasCursorPosition, cursorPosition,
            leftMouseDown, mousePressedThisFrame);
        g_guiRenderTarget->EndScene();
        g_pd3dDevice->SetViewport(&oldViewport);

        g_guiDirty = false;
        SnapshotGuiState(hasCursorPosition, cursorPosition, leftMouseDown);
    }

    g_guiRenderTarget->Blit(static_cast<int>(viewport.X), static_cast<int>(viewport.Y));

    g_leftMouseWasDown = leftMouseDown;

    g_pd3dDevice->ApplyStateBlock(g_dwOldStateBlock);
    g_pd3dDevice->SetVertexShader(g_dwOldVertexShader);
}

void GUI::RenderGreeting()
{
    if (!isInitialized || !g_pd3dDevice || !isGreeting || IsGameLoading())
        return;

    if (!g_dwOldStateBlock)
        CreateStateBlock();

    if (!g_dwOldStateBlock)
        return;

    D3DVIEWPORT8 viewport = {};
    g_pd3dDevice->GetViewport(&viewport);
    if (viewport.Width == 0 || viewport.Height == 0)
        return;

    EnsureRenderTargets(viewport);
    if (!g_greetingRenderTarget)
        return;

    const bool shouldRedraw = ShouldRedrawGreeting();

    g_pd3dDevice->CaptureStateBlock(g_dwOldStateBlock);
    g_pd3dDevice->GetVertexShader(&g_dwOldVertexShader);

    ApplyGuiRenderState();

    if (shouldRedraw)
    {
        D3DVIEWPORT8 oldViewport = viewport;
        g_greetingRenderTarget->BeginScene();
        DrawGreetingContent();
        g_greetingRenderTarget->EndScene();
        g_pd3dDevice->SetViewport(&oldViewport);

        g_greetingDirty = false;
        SnapshotGreetingState();
    }

    g_greetingRenderTarget->Blit(static_cast<int>(viewport.X), static_cast<int>(viewport.Y));

    g_pd3dDevice->ApplyStateBlock(g_dwOldStateBlock);
    g_pd3dDevice->SetVertexShader(g_dwOldVertexShader);
}

void GUI::Shutdown()
{
    Render::Shutdown();

    UpdateMouseCaptureState(false);
    UpdateCursorVisibility(false);

    g_arrowCursor = nullptr;

    if (g_dwOldStateBlock)
    {
        g_pd3dDevice->DeleteStateBlock(g_dwOldStateBlock);
        g_dwOldStateBlock = 0;
    }

    DestroyRenderTargets();
    g_pd3dDevice = nullptr;
    isInitialized = false;
}

void GUI::OnDeviceLost()
{
    if (!g_pd3dDevice)
        return;

    Render::Shutdown();

    if (g_dwOldStateBlock)
    {
        g_pd3dDevice->DeleteStateBlock(g_dwOldStateBlock);
        g_dwOldStateBlock = 0;
    }

    DestroyRenderTargets();
    isInitialized = false;
}

void GUI::OnDeviceReset(LPDIRECT3DDEVICE8 device)
{
    if (device)
        g_pd3dDevice = device;

    if (!g_pd3dDevice)
        return;

    CreateStateBlock();
    Render::Initialise(g_pd3dDevice);
    isInitialized = true;
    g_guiDirty = true;
    g_greetingDirty = true;
}

void GUI::Toggle()
{
    if (!isHost)
        return;

    isVisible = !isVisible;
    g_guiDirty = true;

    if (!isVisible)
    {
        UpdateMouseCaptureState(false);
        UpdateCursorVisibility(false);

        g_fetch.hasResult = false;
        g_fetch.uid = 0;
        g_fetch.player = nullptr;
        g_fetch.displayName.clear();
        g_fetch.online = false;
        g_fetch.ip.clear();
        g_fetch.statusMessage.clear();
    }
}

void GUI::ToogleGreeting()
{
    if (!isHost)
        return;

    if (isGreeting)
        isGreeting = false;
    g_greetingDirty = true;
}
