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

constexpr const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_I20 = 20;

constexpr const UINT_PTR MENU_SUBCLASS_ID = 1;

class BrxDarkMode : public AcRxArxApp
{
    inline static HHOOK m_hMenuHook = nullptr;
    inline static HICON hIcon = LoadIcon(AfxGetInstanceHandle(), MAKEINTRESOURCE(31233));
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
        BOOL USE_DARK_MODE = true;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE_I20, &USE_DARK_MODE, sizeof(USE_DARK_MODE));
        const auto style = GetWindowLong(hwnd, GWL_STYLE);
        SetWindowLong(hwnd, GWL_STYLE, 0);
        SetWindowLong(hwnd, GWL_STYLE, style);
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
            RECT rcWindow{};
            GetWindowRect(hTargetWnd, &rcWindow);
            int winWidth = rcWindow.right - rcWindow.left;

            RECT rcMenuBarStrip{};
            rcMenuBarStrip.left = 0;
            rcMenuBarStrip.right = winWidth;
            rcMenuBarStrip.top = mbi.rcBar.top - rcWindow.top;
            rcMenuBarStrip.bottom = mbi.rcBar.bottom - rcWindow.top;

            rcMenuBarStrip.top += 0;
            rcMenuBarStrip.bottom += 2;

            int stripWidth = rcMenuBarStrip.right - rcMenuBarStrip.left;
            int stripHeight = rcMenuBarStrip.bottom - rcMenuBarStrip.top;

            if (stripWidth <= 0 || stripHeight <= 0)
            {
                ReleaseDC(hTargetWnd, hdcScreen);
                return;
            }

            // --- DOUBLE BUFFERING SETUP ---
            HDC hdcMem = CreateCompatibleDC(hdcScreen);
            HBITMAP hBmpMem = CreateCompatibleBitmap(hdcScreen, stripWidth, stripHeight);
            HBITMAP hBmpOld = (HBITMAP)SelectObject(hdcMem, hBmpMem);

            // --- BASE CANVAS ---
            RECT rcMemStrip = { 0, 0, stripWidth, stripHeight };
            HBRUSH hDarkBrush = CreateSolidBrush(RGB(30, 30, 30)); // Base Dark Canvas
            FillRect(hdcMem, &rcMemStrip, hDarkBrush);
            DeleteObject(hDarkBrush);

            // --- RENDER STRUCTURAL ELEMENTS & HOVER TILES ---
            HMENU hMenu = GetMenu(hTargetWnd);
            if (hMenu)
            {
                int count = GetMenuItemCount(hMenu);
                SetBkMode(hdcMem, TRANSPARENT);

                HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
                HFONT hOldFont = (HFONT)SelectObject(hdcMem, hFont);

                // Create selection highlighting tools
                HBRUSH hHoverBrush = CreateSolidBrush(RGB(50, 50, 50)); // Sleek grey highlight accent

                for (int i = 0; i < count; i++)
                {
                    TCHAR szMenuText[128];
                    GetMenuString(hMenu, i, szMenuText, 128, MF_BYPOSITION);

                    if (_tcslen(szMenuText) == 0) continue;

                    RECT rcItemScreen{};
                    if (GetMenuItemRect(hTargetWnd, hMenu, i, &rcItemScreen))
                    {
                        RECT rcTextSlot;
                        rcTextSlot.left = (rcItemScreen.left - rcWindow.left) - rcMenuBarStrip.left;
                        rcTextSlot.top = 0;
                        rcTextSlot.right = (rcItemScreen.right - rcWindow.left) - rcMenuBarStrip.left;
                        rcTextSlot.bottom = stripHeight;

                        // If this item is being hovered or clicked, paint the highlight background tile first!
                        if (i == activeHoverIdx)
                        {
                            FillRect(hdcMem, &rcTextSlot, hHoverBrush);
                            SetTextColor(hdcMem, RGB(255, 255, 255)); // Bright White text for highlighted item
                        }
                        else
                        {
                            SetTextColor(hdcMem, RGB(210, 210, 210)); // Soft Off-White for neutral items
                        }

                        DrawText(hdcMem, szMenuText, -1, &rcTextSlot, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }
                }

                DeleteObject(hHoverBrush);
                SelectObject(hdcMem, hOldFont);
            }

            // --- SINGLE BLIT ATOMIC TRANSFER ---
            BitBlt(hdcScreen, rcMenuBarStrip.left, rcMenuBarStrip.top, stripWidth, stripHeight,
                hdcMem, 0, 0, SRCCOPY);

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
        // dwRefData will store our currently hovered menu item index.
        // We treat -1 as "no item hovered".
        int currentHoverIdx = (int)dwRefData;

        switch (uMsg)
        {
            case WM_ERASEBKGND:
                return TRUE;

            case WM_NCPAINT:
            case WM_NCACTIVATE:
            {
                LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
                // We pass the hover index down to the paint system
                PerformDarkMenuPaint(hWnd, currentHoverIdx);
                return res;
            }

            case WM_NCMOUSEMOVE:
                if (wParam == HTMENU)
                {
                    // Track mouse coordinates to find out which item we are hovering over
                    POINT pt;
                    GetCursorPos(&pt);

                    HMENU hMenu = GetMenu(hWnd);
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

                    // Only repaint if the hover target actually shifted to save CPU cycles
                    if (newHoverIdx != currentHoverIdx)
                    {
                        SetWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass, (DWORD_PTR)newHoverIdx);
                        PerformDarkMenuPaint(hWnd, newHoverIdx);
                    }
                    return 0;
                }
                break;

            case WM_NCMOUSELEAVE:
            case WM_MOUSELEAVE:
                if (currentHoverIdx != -1)
                {
                    SetWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass, (DWORD_PTR)-1);
                    PerformDarkMenuPaint(hWnd, -1);
                }
                break;

            case WM_MENUSELECT:
            {
                UINT uItem = (UINT)LOWORD(wParam);
                UINT uFlags = (UINT)HIWORD(wParam);
                HMENU hMenu = (HMENU)lParam;

                if (!(uFlags & MF_SYSMENU) && hMenu == GetMenu(hWnd))
                {
                    int activeIdx = -1;
                    int count = GetMenuItemCount(hMenu);
                    for (int i = 0; i < count; i++)
                    {
                        if (GetSubMenu(hMenu, i) == (HMENU)uItem || i == (int)uItem)
                        {
                            activeIdx = i;
                            break;
                        }
                    }
                    if (activeIdx != -1)
                    {
                        SetWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass, (DWORD_PTR)activeIdx);
                        PerformDarkMenuPaint(hWnd, activeIdx);
                    }
                }
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);
            }

            case WM_ENTERMENULOOP:
                PerformDarkMenuPaint(hWnd, currentHoverIdx);
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);

            case WM_EXITMENULOOP:
                SetWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass, (DWORD_PTR)-1);
                PerformDarkMenuPaint(hWnd, -1);
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);

            case WM_NCLBUTTONDOWN:
            case WM_NCLBUTTONDBLCLK:
                if (wParam == HTMENU)
                {
                    PerformDarkMenuPaint(hWnd, currentHoverIdx);
                    LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
                    PerformDarkMenuPaint(hWnd, currentHoverIdx);
                    return res;
                }
                break;

            case WM_SETCURSOR:
                if (LOWORD(lParam) == HTMENU)
                {
                    // Trigger tracking so WM_NCMOUSELEAVE fires properly when leaving the NC area
                    TRACKMOUSEEVENT tme = { sizeof(TRACKMOUSEEVENT), TME_LEAVE | TME_NONCLIENT, hWnd, HOVER_DEFAULT };
                    TrackMouseEvent(&tme);

                    PerformDarkMenuPaint(hWnd, currentHoverIdx);
                    return TRUE;
                }
                break;

            case WM_NCHITTEST:
                return DefSubclassProc(hWnd, uMsg, wParam, lParam);

            case WM_NCDESTROY:
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
                SetWindowSubclass(hMainWnd, DarkMenuBarSubclassProc, MENU_SUBCLASS_ID , (DWORD_PTR)-1);

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
    static void tryApplyTheme(HWND hwndTarget, LONG_PTR style)
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
                ::SetWindowLongPtr(hwndTarget, GWL_STYLE, style | WS_POPUPWINDOW);
                ::SendMessage(hwndTarget, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
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
                    tryApplyTheme(pCwprs->hwnd, style);
            }
        }
        return ::CallNextHookEx(m_hMenuHook, nCode, wParam, lParam);
    }

    void InstallThemeHook()
    {
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