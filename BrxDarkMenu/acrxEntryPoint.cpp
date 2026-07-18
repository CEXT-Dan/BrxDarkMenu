// (C) Copyright 2002-2007 by Autodesk, Inc. 
//
// Permission to use, copy, modify, and distribute this software in
// object code form for any purpose and without fee is hereby granted, 
// provided that the above copyright notice appears in all copies and 
// that both that copyright notice and the limited warranty and
// restricted rights notice below appear in all supporting 
// documentation.
//
// AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS. 
// AUTODESK SPECIFICALLY DISCLAIMS ANY IMPLIED WARRANTY OF
// MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE.  AUTODESK, INC. 
// DOES NOT WARRANT THAT THE OPERATION OF THE PROGRAM WILL BE
// UNINTERRUPTED OR ERROR FREE.
//
// Use, duplication, or disclosure by the U.S. Government is subject to 
// restrictions set forth in FAR 52.227-19 (Commercial Computer
// Software - Restricted Rights) and DFAR 252.227-7013(c)(1)(ii)
// (Rights in Technical Data and Computer Software), as applicable.
//

#include "StdAfx.h"
#include "resource.h"
#include "dwmapi.h"

constexpr const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_I20 = 20; //dark
constexpr const UINT_PTR MENU_LEAVE_TIMER_ID = 4242;
constexpr const UINT_PTR MENU_UNHOVER_TIMER_ID = 4243;
constexpr const UINT MENU_UNHOVER_DELAY_MS = 60;
constexpr const UINT MENU_LEAVE_TIMER_DELAY_MS = 20;
constexpr const UINT_PTR MENU_SUBCLASS_ID = 1;
constexpr const int MENU_UNHOVER_PENDING = -2;

#ifndef WM_UAHDRAWMENU
#define WM_UAHDRAWMENU       0x0091
#define WM_UAHDRAWMENUITEM   0x0092
#endif

class BrxDarkMode : public AcRxArxApp
{
    inline static HHOOK m_hMenuHook = nullptr;
    inline static HICON m_hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(31233));
    inline static HICON m_hIconMenu = (HICON)::LoadImage(
        AfxGetInstanceHandle(),
        MAKEINTRESOURCE(32000),
        IMAGE_ICON,
        ::GetSystemMetrics(SM_CXSMICON),
        ::GetSystemMetrics(SM_CYSMICON),
        LR_DEFAULTCOLOR | LR_SHARED
    );

public:
    BrxDarkMode() : AcRxArxApp()
    {}

    virtual AcRx::AppRetCode On_kInitAppMsg(void* pkt)
    {
        AcRx::AppRetCode retCode = AcRxArxApp::On_kInitAppMsg(pkt);
        acrxLockApplication(pkt);
        setTitleThemeDark(adsw_acadMainWnd());
        InstallThemeHook();
        InitializeDarkMenuBar();
        return (retCode);
    }

    virtual AcRx::AppRetCode On_kUnloadAppMsg(void* pkt)
    {
        AcRx::AppRetCode retCode = AcRxArxApp::On_kUnloadAppMsg(pkt);
        if (m_hMenuHook != nullptr)
        {
            UnhookWindowsHookEx(m_hMenuHook);
            m_hMenuHook = nullptr;
        }

        HWND hMainWnd = adsw_acadMainWnd();
        if (hMainWnd != nullptr)
        {
            // since we call acrxLockApplication, we only get here on app exit
            // there's no need to restore the previous theme 
            RemoveWindowSubclass(hMainWnd, DarkMenuBarSubclassProc, MENU_SUBCLASS_ID);
        }
        return (retCode);
    }

    virtual AcRx::AppRetCode On_kLoadDwgMsg(void* pkt) override
    {
        AcRx::AppRetCode retCode = AcRxDbxApp::On_kLoadDwgMsg(pkt);
        return retCode;
    }

    static void setTitleThemeDark(HWND hwnd)
    {
        if (!IsWindow(hwnd))
            return;

        const BOOL useDarkMode = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_I20, &useDarkMode, sizeof(useDarkMode));
        RedrawWindow(hwnd, nullptr, nullptr, RDW_FRAME | RDW_INVALIDATE);
    }

    // This just paints over the menubar, flashy eh
    static void PerformDarkMenuPaint(HWND hTargetWnd, int activeHoverIdx)
    {
        HDC hdcScreen = GetWindowDC(hTargetWnd);
        if (!hdcScreen)
            return;

        MENUBARINFO mbi = { 0 };
        mbi.cbSize = sizeof(MENUBARINFO);

        if (GetMenuBarInfo(hTargetWnd, OBJID_MENU, 0, &mbi))
        {
            UINT dpi = ::GetDpiForWindow(hTargetWnd);

            RECT rcWindow{};
            GetWindowRect(hTargetWnd, &rcWindow);
            int winWidth = rcWindow.right - rcWindow.left;

            RECT rcMenuBarStrip{};
            rcMenuBarStrip.left = 0;
            rcMenuBarStrip.right = winWidth;
            rcMenuBarStrip.top = mbi.rcBar.top - rcWindow.top;
            rcMenuBarStrip.bottom = mbi.rcBar.bottom - rcWindow.top;

            // --- DPI SCALE MAGIC NUMBERS ---e
            int verticalPadding = MulDiv(2, dpi, 96);
            rcMenuBarStrip.bottom += verticalPadding;

            int stripWidth = rcMenuBarStrip.right - rcMenuBarStrip.left;
            int stripHeight = rcMenuBarStrip.bottom - rcMenuBarStrip.top;

            if (stripWidth <= 0 || stripHeight <= 0)
            {
                ReleaseDC(hTargetWnd, hdcScreen);
                return;
            }

            // --- DOUBLE BUFFERING SETUP ---
            HDC hdcMem = CreateCompatibleDC(hdcScreen);
            if (!hdcMem)
            {
                ReleaseDC(hTargetWnd, hdcScreen);
                return;
            }

            HBITMAP hBmpMem = CreateCompatibleBitmap(hdcScreen, stripWidth, stripHeight);
            if (!hBmpMem)
            {
                DeleteDC(hdcMem);
                ReleaseDC(hTargetWnd, hdcScreen);
                return;
            }

            HBITMAP hBmpOld = (HBITMAP)SelectObject(hdcMem, hBmpMem);
            if (!hBmpOld || hBmpOld == HGDI_ERROR)
            {
                DeleteObject(hBmpMem);
                DeleteDC(hdcMem);
                ReleaseDC(hTargetWnd, hdcScreen);
                return;
            }

            // --- BASE CANVAS ---
            RECT rcMemStrip = { 0, 0, stripWidth, stripHeight };
            HBRUSH hDarkBrush = CreateSolidBrush(RGB(30, 30, 30));
            if (hDarkBrush)
            {
                FillRect(hdcMem, &rcMemStrip, hDarkBrush);
                DeleteObject(hDarkBrush);
            }

            // --- RENDER STRUCTURAL ELEMENTS & HOVER TILES FIRST ---
            HMENU hMenu = GetMenu(hTargetWnd);
            if (hMenu)
            {
                int count = GetMenuItemCount(hMenu);
                SetBkMode(hdcMem, TRANSPARENT);

                // --- DPI-AWARE DYNAMIC SYSTEM FONT LOOKUP ---
                NONCLIENTMETRICS ncm = { 0 };
                ncm.cbSize = sizeof(NONCLIENTMETRICS);
                HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
                bool ownsFont = false;

                if (::SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &ncm, 0, dpi))
                {
                    if (HFONT dpiMenuFont = CreateFontIndirect(&ncm.lfMenuFont))
                    {
                        hFont = dpiMenuFont;
                        ownsFont = true;
                    }
                }

                HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);
                HBRUSH hHoverBrush = CreateSolidBrush(RGB(50, 50, 50));

                for (int i = 0; i < count; i++)
                {
                    TCHAR szMenuText[128];
                    GetMenuString(hMenu, i, szMenuText, 128, MF_BYPOSITION);

                    if (_tcslen(szMenuText) == 0)
                        continue;

                    RECT rcItemScreen{};
                    if (GetMenuItemRect(hTargetWnd, hMenu, i, &rcItemScreen))
                    {
                        RECT rcTextSlot;
                        rcTextSlot.left = (rcItemScreen.left - rcWindow.left) - rcMenuBarStrip.left;
                        rcTextSlot.top = 0;
                        rcTextSlot.right = (rcItemScreen.right - rcWindow.left) - rcMenuBarStrip.left;
                        rcTextSlot.bottom = stripHeight;

                        if (i == activeHoverIdx)
                        {
                            if (hHoverBrush)
                                FillRect(hdcMem, &rcTextSlot, hHoverBrush);
                            SetTextColor(hdcMem, RGB(255, 255, 255));
                        }
                        else
                        {
                            SetTextColor(hdcMem, RGB(210, 210, 210));
                        }

                        DrawText(hdcMem, szMenuText, -1, &rcTextSlot, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }
                }

                if (hHoverBrush)
                    DeleteObject(hHoverBrush);
                if (hOldFont && hOldFont != HGDI_ERROR)
                    SelectObject(hdcMem, hOldFont);

                if (ownsFont)
                {
                    DeleteObject(hFont);
                }
            }

            // --- DRAW THE CUSTOM MENU ICON LAST (OVERLAY) ---
            int iconDim = ::GetSystemMetricsForDpi(SM_CXSMICON, dpi);
            int iconX = (iconDim * 2) / 2;
            int iconY = (stripHeight - iconDim) / 2;

            if (hMenu && m_hIconMenu)
            {
                DrawIconEx(hdcMem, iconX, iconY, m_hIconMenu, iconDim, iconDim, 0, NULL, DI_NORMAL);
            }

            // --- SINGLE BLIT ATOMIC TRANSFER ---
            BitBlt(hdcScreen, rcMenuBarStrip.left, rcMenuBarStrip.top, stripWidth, stripHeight, hdcMem, 0, 0, SRCCOPY);

            // --- CLEAN CLEANUP ---
            SelectObject(hdcMem, hBmpOld);
            DeleteObject(hBmpMem);
            DeleteDC(hdcMem);
        }
        ReleaseDC(hTargetWnd, hdcScreen);
    }

    static LRESULT CALLBACK DarkMenuBarSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
        UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
    {
        int currentHoverIdx = (int)dwRefData;

        switch (uMsg)
        {
            case WM_ERASEBKGND:
                return TRUE;

            case WM_NCPAINT:
            case WM_NCACTIVATE:
            {
                // Execute the base frame layout (borders, drop shadows, minimize/maximize buttons)
                LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);

                // Immediately paint your dark menu over the white asset 
                // before the graphics card flushes the frame buffer to the monitor.
                if (currentHoverIdx != MENU_UNHOVER_PENDING)
                {
                    PerformDarkMenuPaint(hWnd, currentHoverIdx);
                }
                return res;
            }

            case WM_UAHDRAWMENU:
                PerformDarkMenuPaint(hWnd, currentHoverIdx);
                return 0;  // Do not let the native light menu draw.

            case WM_UAHDRAWMENUITEM:
                return 0;  // The full custom paint above draws all menu text/items.

            case WM_INITMENUPOPUP:
            {
                LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
                PerformDarkMenuPaint(hWnd, currentHoverIdx);
                return res;
            }

            case WM_NCMOUSEMOVE:
                if (wParam == HTMENU)
                {
                    KillTimer(hWnd, MENU_UNHOVER_TIMER_ID);

                    POINT pt;
                    GetCursorPos(&pt);

                    HMENU hMenu = GetMenu(hWnd);
                    if (!hMenu)
                        return DefSubclassProc(hWnd, uMsg, wParam, lParam);

                    int count = GetMenuItemCount(hMenu);
                    int newHoverIdx = -1;

                    for (int i = 0; i < count; i++)
                    {
                        RECT rcItem;
                        if (GetMenuItemRect(hWnd, hMenu, i, &rcItem))
                        {
                            if (PtInRect(&rcItem, pt))
                            {
                                newHoverIdx = i;
                                break;
                            }
                        }
                    }

                    if (newHoverIdx != currentHoverIdx)
                    {
                        SetWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass, (DWORD_PTR)newHoverIdx);
                        PerformDarkMenuPaint(hWnd, newHoverIdx);
                    }

                    SetTimer(hWnd, MENU_LEAVE_TIMER_ID, MENU_LEAVE_TIMER_DELAY_MS, NULL);
                    return 0;
                }
                break; // Standard non-client moves must continue down to DefSubclassProc

            case WM_NCMOUSELEAVE:
                KillTimer(hWnd, MENU_LEAVE_TIMER_ID);
                if (currentHoverIdx != -1 && currentHoverIdx != MENU_UNHOVER_PENDING)
                {
                    SetWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass, (DWORD_PTR)MENU_UNHOVER_PENDING);
                    SetTimer(hWnd, MENU_UNHOVER_TIMER_ID, MENU_UNHOVER_DELAY_MS, NULL);
                }
                break;

            case WM_TIMER:
                if (wParam == MENU_LEAVE_TIMER_ID)
                {
                    POINT pt;
                    GetCursorPos(&pt);

                    LRESULT hitTest = DefSubclassProc(hWnd, WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y));
                    if (hitTest != HTMENU)
                    {
                        KillTimer(hWnd, MENU_LEAVE_TIMER_ID);

                        if (currentHoverIdx != -1 && currentHoverIdx != MENU_UNHOVER_PENDING)
                        {
                            SetWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass, (DWORD_PTR)MENU_UNHOVER_PENDING);
                            SetTimer(hWnd, MENU_UNHOVER_TIMER_ID, MENU_UNHOVER_DELAY_MS, NULL);
                        }
                    }
                    return 0;
                }
                if (wParam == MENU_UNHOVER_TIMER_ID)
                {
                    KillTimer(hWnd, MENU_UNHOVER_TIMER_ID);

                    POINT pt;
                    GetCursorPos(&pt);
                    LRESULT hitTest = DefSubclassProc(hWnd, WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y));
                    if (hitTest != HTMENU)
                    {
                        SetWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass, (DWORD_PTR)-1);
                        PerformDarkMenuPaint(hWnd, -1);
                    }
                    return 0;
                }
                break;

            case WM_MENUSELECT:
            {
                UINT uItem = (UINT)LOWORD(wParam);
                UINT uFlags = (UINT)HIWORD(wParam);
                HMENU hMenu = (HMENU)lParam;
                if (hMenu == nullptr && uItem == 0xFFFF && uFlags == 0xFFFF)
                {
                    if (currentHoverIdx != -1)
                    {
                        SetWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass, (DWORD_PTR)-1);
                        PerformDarkMenuPaint(hWnd, -1);
                    }
                }
                else if (!(uFlags & MF_SYSMENU) && (uFlags & MF_POPUP) && hMenu == GetMenu(hWnd))
                {
                    int count = GetMenuItemCount(hMenu);
                    if (uItem < (UINT)count)
                    {
                        SetWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass, (DWORD_PTR)uItem);
                        PerformDarkMenuPaint(hWnd, (int)uItem);
                    }
                }
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);
            }

            case WM_ENTERMENULOOP:
                PerformDarkMenuPaint(hWnd, currentHoverIdx);
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);

            case WM_EXITMENULOOP:
                KillTimer(hWnd, MENU_LEAVE_TIMER_ID);
                KillTimer(hWnd, MENU_UNHOVER_TIMER_ID);
                SetWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass, (DWORD_PTR)-1);
                PerformDarkMenuPaint(hWnd, -1);
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);

            case WM_SETCURSOR:
                if (LOWORD(lParam) == HTMENU)
                {
                    TRACKMOUSEEVENT tme = {
                        sizeof(TRACKMOUSEEVENT),
                        TME_LEAVE | TME_NONCLIENT,
                        hWnd,
                        HOVER_DEFAULT
                    };
                    TrackMouseEvent(&tme);
                    return TRUE;
                }
                break;

            case WM_NCHITTEST:
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);

            case WM_NCDESTROY:
                KillTimer(hWnd, MENU_LEAVE_TIMER_ID);
                KillTimer(hWnd, MENU_UNHOVER_TIMER_ID);
                RemoveWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass);
                break;
        }
        return DefSubclassProc(hWnd, uMsg, wParam, lParam);
    }

    static void InitializeDarkMenuBar()
    {
        enum PreferredAppMode { Default = 0, AllowDark = 1, ForceDark = 2, ForceLight = 3, Max = 4 };
        typedef bool (WINAPI* fnAllowDarkModeForWindow)(HWND hWnd, bool allow);
        typedef PreferredAppMode(WINAPI* fnSetPreferredAppMode)(PreferredAppMode appMode);
        typedef void (WINAPI* fnFlushMenuThemes)();

        HMODULE hUxTheme = GetModuleHandle(_T("uxtheme.dll"));
        if (!hUxTheme) return;

        auto SetPreferredAppMode = (fnSetPreferredAppMode)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(135));
        auto AllowDarkModeForWindow = (fnAllowDarkModeForWindow)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(133));
        auto FlushMenuThemes = (fnFlushMenuThemes)GetProcAddress(hUxTheme, MAKEINTRESOURCEA(136));

        // Force dropdowns dark using the hidden Win32 APIs
        if (SetPreferredAppMode && AllowDarkModeForWindow && FlushMenuThemes)
        {
            SetPreferredAppMode(PreferredAppMode::ForceDark);

            HWND hMainWnd = adsw_acadMainWnd(); // BricsCAD MFC Frame Window
            if (hMainWnd != NULL)
            {
                AllowDarkModeForWindow(hMainWnd, true);
                FlushMenuThemes();

                // Inject the window subclass hook to fix the top horizontal menu strip
                // The parameter '1' is a unique Identifier for this specific subclass hook
                SetWindowSubclass(hMainWnd, DarkMenuBarSubclassProc, MENU_SUBCLASS_ID, (DWORD_PTR)-1);

                // Force a total window repaint to trigger the new paint sequence instantly
                SetWindowPos(hMainWnd, NULL, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

                // after acadMainWnd menus are set, switch back to light mode.
                // don't  FlushMenuThemes();
                AllowDarkModeForWindow(hMainWnd, false);
            }
        }
    }

    // Set dialogs (model) with a dark title bar and apply icon
    static void tryApplyTheme(HWND hwndTarget)
    {
        TCHAR className[256];
        if (::GetClassName(hwndTarget, className, 256) > 0)
        {
            // nave cube
            if (className[0] == 'Q' && className[1] == 't')
                return;

            BOOL useDarkMode = TRUE;
            ::DwmSetWindowAttribute(hwndTarget, DWMWA_USE_IMMERSIVE_DARK_MODE_I20, &useDarkMode, sizeof(useDarkMode));

            HICON hExistingIcon = (HICON)::SendMessage(hwndTarget, WM_GETICON, ICON_SMALL, 0);
            if (hExistingIcon == NULL)
                hExistingIcon = (HICON)::GetClassLongPtr(hwndTarget, GCLP_HICONSM);

            if (hExistingIcon == NULL)
            {
                ::SendMessage(hwndTarget, WM_SETICON, ICON_SMALL, (LPARAM)m_hIcon);
            }
        }
    }

    static LRESULT CALLBACK MenuWindowHookProc(int nCode, WPARAM wParam, LPARAM lParam)
    {
        if (nCode >= 0 && lParam != 0)
        {
            CWPRETSTRUCT* pCwprs = (CWPRETSTRUCT*)lParam;

            if (pCwprs->message == WM_CREATE)
            {
                LONG_PTR style = ::GetWindowLongPtr(pCwprs->hwnd, GWL_STYLE);
                if (!(style & WS_CHILD))
                    tryApplyTheme(pCwprs->hwnd);
            }
        }
        return ::CallNextHookEx(m_hMenuHook, nCode, wParam, lParam);
    }

    void InstallThemeHook()
    {
        if (!m_hMenuHook)
            m_hMenuHook = SetWindowsHookEx(WH_CALLWNDPROCRET, MenuWindowHookProc, NULL, GetCurrentThreadId());
    }

    virtual void RegisterServerComponents()
    {}

#ifdef DARK_TEST
    static void BrxDarkMode_dodark(void)
    {}
#endif
};

//-----------------------------------------------------------------------------
IMPLEMENT_ARX_ENTRYPOINT(BrxDarkMode)
#ifdef DARK_TEST
ACED_ARXCOMMAND_ENTRY_AUTO(BrxDarkMode, BrxDarkMode, _dodark, dodark, ACRX_CMD_MODAL, NULL)
#endif
