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
#include <unordered_set>
#include "dwmapi.h"

constexpr const DWORD DWMWA_USE_IMMERSIVE_DARK_MODE_I20 = 20;

class BrxDarkMode : public AcRxArxApp
{
    inline static std::unordered_set<HWND> m_winmap;
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
        InstallContextMenuHook();
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
            RemoveWindowSubclass(hMainWnd, DarkMenuBarSubclassProc, 1);
        }
        m_winmap.clear();
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
    static void PerformDarkMenuPaint(HWND hTargetWnd)
    {
        HDC hdc = GetWindowDC(hTargetWnd);
        if (!hdc)
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

            // Applying your perfect layout alignment offsets
            rcMenuBarStrip.top += 0;
            rcMenuBarStrip.bottom += 2;

            // 1. Draw solid dark background canvas
            HBRUSH hDarkBrush = CreateSolidBrush(RGB(30, 30, 30));
            FillRect(hdc, &rcMenuBarStrip, hDarkBrush);
            DeleteObject(hDarkBrush);

            // 2. Re-render structural strings centered over their true hit-test targets
            HMENU hMenu = GetMenu(hTargetWnd);
            if (hMenu)
            {
                int count = GetMenuItemCount(hMenu);
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(240, 240, 240)); // Clean Off-White

                HFONT hFont = (HFONT)GetStockObject(DEFAULT_GUI_FONT);
                HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);

                for (int i = 0; i < count; i++)
                {
                    TCHAR szMenuText[128];
                    GetMenuString(hMenu, i, szMenuText, 128, MF_BYPOSITION);

                    if (_tcslen(szMenuText) == 0) continue;

                    RECT rcItemScreen{};
                    if (GetMenuItemRect(hTargetWnd, hMenu, i, &rcItemScreen))
                    {
                        RECT rcTextSlot;
                        rcTextSlot.left = rcItemScreen.left - rcWindow.left;
                        rcTextSlot.top = rcMenuBarStrip.top;
                        rcTextSlot.right = rcItemScreen.right - rcWindow.left;
                        rcTextSlot.bottom = rcMenuBarStrip.bottom;
                        DrawText(hdc, szMenuText, -1, &rcTextSlot, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
                    }
                }
                SelectObject(hdc, hOldFont);
            }
        }
        ReleaseDC(hTargetWnd, hdc);
    }

    static LRESULT CALLBACK DarkMenuBarSubclassProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
        UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
    {
        // Capture core draw and window focus events
        if (uMsg == WM_NCPAINT || uMsg == WM_NCACTIVATE || uMsg == WM_MENUSELECT ||
            uMsg == WM_ENTERMENULOOP || uMsg == WM_EXITMENULOOP || uMsg == WM_MOUSEMOVE)
        {
            LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
            PerformDarkMenuPaint(hWnd);
            return res;
        }

        // Intercept mouse glide globally across the bar to wipe hot-track artifacts
        if (uMsg == WM_NCMOUSEMOVE)
        {
            if (wParam == HTMENU)
            {
                PerformDarkMenuPaint(hWnd);
                return 0;
            }
        }

        // Keep background solid dark when empty bar areas are clicked
        if (uMsg == WM_NCLBUTTONDOWN || uMsg == WM_NCLBUTTONDBLCLK)
        {
            if (wParam == HTMENU)
            {
                LRESULT res = DefSubclassProc(hWnd, uMsg, wParam, lParam);
                PerformDarkMenuPaint(hWnd);
                return res;
            }
        }

        // Stop Windows from executing the default high-priority light hover overlay loop
        if (uMsg == WM_SETCURSOR)
        {
            if (LOWORD(lParam) == HTMENU)
            {
                PerformDarkMenuPaint(hWnd);
                return TRUE;
            }
        }

        if (uMsg == WM_NCHITTEST)
        {
            return DefSubclassProc(hWnd, uMsg, wParam, lParam);
        }

        if (uMsg == WM_NCDESTROY)
        {
            RemoveWindowSubclass(hWnd, DarkMenuBarSubclassProc, uIdSubclass);
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
                SetWindowSubclass(hMainWnd, DarkMenuBarSubclassProc, 1, 0);

                // Force a total window repaint to trigger the new paint sequence instantly
                SetWindowPos(hMainWnd, NULL, 0, 0, 0, 0,
                    SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
            }
        }
    }

    // This wis an attempt to paint Editor right click context menus correctly 
    // it's a side effect of 'DarkMain' in that they are white 
    // Also, we can catch title bars here (modal?)
    static void tryApplyTheme(HWND hwndTarget, LONG_PTR style)
    {
        TCHAR className[256];
        if (::GetClassName(hwndTarget, className, 256) > 0)
        {
            bool isSystemMenu = (_tcscmp(className, _T("#32768")) == 0);
            bool isBricscadMainWindow = (_tcscmp(className, _T("BricscadMainWindow")) == 0);

            if (isSystemMenu || isBricscadMainWindow)
            {
                ::SetWindowTheme(hwndTarget, _T("DarkMain"), NULL);
            }
            else
            {
                BOOL useDarkMode = TRUE;
                ::DwmSetWindowAttribute(hwndTarget, DWMWA_USE_IMMERSIVE_DARK_MODE_I20, &useDarkMode, sizeof(useDarkMode));

                HICON hExistingIcon = (HICON)::SendMessage(hwndTarget, WM_GETICON, ICON_SMALL, 0);
                if (hExistingIcon == NULL)
                {
                    hExistingIcon = (HICON)::GetClassLongPtr(hwndTarget, GCLP_HICONSM);
                }
                if (hExistingIcon == NULL)
                {
                    ::SetWindowLongPtr(hwndTarget, GWL_STYLE, style | WS_POPUPWINDOW);
                    ::SendMessage(hwndTarget, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
                }
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
                HWND hwndTarget = pCwprs->hwnd;
                LONG_PTR style = ::GetWindowLongPtr(hwndTarget, GWL_STYLE);

                if (!(style & WS_CHILD) && !m_winmap.contains(hwndTarget))
                {
                    m_winmap.insert(hwndTarget);
                    tryApplyTheme(hwndTarget, style);
                }
            }
        }
        return ::CallNextHookEx(m_hMenuHook, nCode, wParam, lParam);
    }

    void InstallContextMenuHook()
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