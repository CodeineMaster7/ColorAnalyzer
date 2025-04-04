#include <windows.h>
#include <gdiplus.h>
#include <shellapi.h>
#include <string>
#include <commctrl.h>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "comctl32.lib")

#define WM_TRAYICON (WM_USER + 1)
#define ID_HOTKEY_SAVE 1
#define ID_BTN_CLOSE 2
#define ID_BTN_MINIMIZE 3
#define IDT_UPDATE_COLOR 1
#define IDT_CLOSE_SETTINGS 2
#define IDT_CLOSE_MSGBOX 3

HWND hwnd, settingsHwnd;
HWND hMsgWnd = NULL; // добавить рядом с имеющимися глобальными переменными
NOTIFYICONDATA nid = {};
COLORREF currentColor = RGB(255, 255, 255);
HICON hIcon;
UINT saveColorHotkey = MOD_CONTROL | MOD_SHIFT;
UINT saveColorKey = 'C';

// Загружаем иконку из PNG-файла
HICON LoadPngIcon(const wchar_t* filename) {
    Gdiplus::Bitmap bitmap(filename);
    HICON hIcon = NULL;
    bitmap.GetHICON(&hIcon);
    return hIcon;
}

// Получаем цвет под курсором
COLORREF getColorAtCursor() {
    POINT p;
    GetCursorPos(&p);
    HDC hdc = GetDC(NULL);
    COLORREF color = GetPixel(hdc, p.x, p.y);
    ReleaseDC(NULL, hdc);
    return color;
}

// Обновляем цвет иконки и окна
void updateColor() {
    static DWORD lastTrayUpdate = 0;

    currentColor = getColorAtCursor();

    DWORD now = GetTickCount();
    if (now - lastTrayUpdate > 1000) { // Трей обновляем не чаще раза в секунду!
        char hexColor[16];
        sprintf(hexColor, "#%02X%02X%02X", GetRValue(currentColor), GetGValue(currentColor), GetBValue(currentColor));
        strncpy(nid.szTip, hexColor, sizeof(nid.szTip)-1);
        Shell_NotifyIcon(NIM_MODIFY, &nid);
        lastTrayUpdate = now;
    }

    // Перемещение окна происходит без лишней перерисовки:
    POINT p;
    GetCursorPos(&p);
    SetWindowPos(hwnd, HWND_TOPMOST, p.x + 20, p.y, 120, 50, SWP_NOZORDER | SWP_NOSIZE | SWP_NOREDRAW);

    // Перерисовка окна всё-таки нужна чтобы цвет обновился:
    InvalidateRect(hwnd, NULL, FALSE);
}

// Создаем иконку в трее
void addTrayIcon(HWND hwnd) {
    nid.cbSize = sizeof(NOTIFYICONDATA);
    nid.hWnd = hwnd;
    nid.uID = 1;
    nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAYICON;
    hIcon = LoadPngIcon(L"colorpicker.png");  // Загружаем иконку
    nid.hIcon = hIcon ? hIcon : LoadIcon(NULL, IDI_APPLICATION);
    strcpy_s(nid.szTip, "ColorAnalyzer");
    Shell_NotifyIcon(NIM_ADD, &nid);
}

// Удаляем иконку из трея
void removeTrayIcon() {
    Shell_NotifyIcon(NIM_DELETE, &nid);
    if (hIcon) DestroyIcon(hIcon);
}

LRESULT CALLBACK MsgWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            RECT rc;
            GetClientRect(hwnd, &rc);
            DrawTextA(hdc, "Color saved!", -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            EndPaint(hwnd, &ps);
        } break;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

LRESULT CALLBACK SettingsProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_COMMAND:
            if (LOWORD(wParam) == ID_BTN_CLOSE) {
                PostQuitMessage(0);
            } else if (LOWORD(wParam) == ID_BTN_MINIMIZE) {
                ShowWindow(hwnd, SW_MINIMIZE);
            }
            break;
        case WM_CLOSE:
            ShowWindow(hwnd, SW_HIDE);
            return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void CopyTextToClipboard(const char* text) {
    if (OpenClipboard(hwnd)) {
        EmptyClipboard();
        size_t len = strlen(text) + 1;
        HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
        if (hMem) {
            memcpy(GlobalLock(hMem), text, len);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
        CloseClipboard();
    }
}

// Обработчик сообщений окна
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    switch (uMsg) {
        case WM_HOTKEY:
            if (wParam == ID_HOTKEY_SAVE) {
                char hexColor[16];
                sprintf(hexColor, "#%02X%02X%02X", GetRValue(currentColor), GetGValue(currentColor), GetBValue(currentColor));
                CopyTextToClipboard(hexColor);
                
                // Создаем окно вручную вместо MessageBox
                hMsgWnd = CreateWindowExA(WS_EX_TOPMOST, "MyMsgBox", "Info",
                                            WS_POPUP | WS_BORDER | WS_VISIBLE,
                                            CW_USEDEFAULT, CW_USEDEFAULT, 200, 100,
                                            hwnd, NULL, GetModuleHandle(NULL), NULL);
                // Центрируем его на экране:
                RECT rc;
                GetWindowRect(hMsgWnd, &rc);
                int x = (GetSystemMetrics(SM_CXSCREEN) - (rc.right - rc.left)) / 2;
                int y = (GetSystemMetrics(SM_CYSCREEN) - (rc.bottom - rc.top)) / 2;
                SetWindowPos(hMsgWnd, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_SHOWWINDOW);
                        
                SetTimer(hwnd, IDT_CLOSE_MSGBOX, 1000, NULL); // Таймер 1 секунда
                SetTimer(hwnd, IDT_CLOSE_SETTINGS, 2000, NULL); // если нужно для скрытия настроек
            }
            break;

        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
        
            RECT rect;
            GetClientRect(hwnd, &rect);
            int width = rect.right - rect.left;
            int height = rect.bottom - rect.top;
        
            HDC memDC = CreateCompatibleDC(hdc);
            HBITMAP memBmp = CreateCompatibleBitmap(hdc, width, height);
            SelectObject(memDC, memBmp);
        
            // Рисуем в memory DC (без мерцания)
            HBRUSH hBrush = CreateSolidBrush(currentColor);
            FillRect(memDC, &rect, hBrush);
            DeleteObject(hBrush);
        
            char hexColor[16];
            sprintf(hexColor, "#%02X%02X%02X", GetRValue(currentColor), GetGValue(currentColor), GetBValue(currentColor));
            COLORREF textColor = (GetRValue(currentColor) + GetGValue(currentColor) + GetBValue(currentColor) < 382)
                                    ? RGB(255,255,255) : RGB(0,0,0);
            SetBkMode(memDC, TRANSPARENT);
            SetTextColor(memDC, textColor);
            DrawTextA(memDC, hexColor, -1, &rect, DT_CENTER|DT_VCENTER|DT_SINGLELINE);
        
            // Копируем за раз в реальный DC
            BitBlt(hdc, 0, 0, width, height, memDC, 0, 0, SRCCOPY);
        
            DeleteObject(memBmp);
            DeleteDC(memDC);
            EndPaint(hwnd, &ps);
        } break;

        case WM_TIMER:
            switch (wParam) {
                case IDT_CLOSE_MSGBOX:
                    if (hMsgWnd) {
                        DestroyWindow(hMsgWnd);
                        hMsgWnd = NULL;
                    }
                    KillTimer(hwnd, IDT_CLOSE_MSGBOX);
                    break;
                
                case IDT_CLOSE_SETTINGS:
                    ShowWindow(settingsHwnd, SW_HIDE);
                    KillTimer(hwnd, IDT_CLOSE_SETTINGS);
                    break;
                
                case IDT_UPDATE_COLOR:
                    updateColor();
                    break;
            }
            return 0;
    

        case WM_TRAYICON:
            if (lParam == WM_RBUTTONUP) {
                HMENU hMenu = CreatePopupMenu();
                AppendMenu(hMenu, MF_STRING, 1, "Settings");
                AppendMenu(hMenu, MF_STRING, 2, "Exit Analyzer");
                POINT p;
                GetCursorPos(&p);
                SetForegroundWindow(hwnd);
                int cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_NONOTIFY, p.x, p.y, 0, hwnd, NULL);
                DestroyMenu(hMenu);
                if (cmd == 1) ShowWindow(settingsHwnd, SW_SHOW);
                if (cmd == 2) PostQuitMessage(0);
            }
            return 0;

        case WM_DESTROY:
            KillTimer(hwnd, 1);
            removeTrayIcon();
            PostQuitMessage(0);
            return 0;

        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
    return 0;
}

void createSettingsWindow(HINSTANCE hInstance) {
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = SettingsProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "SettingsWindow";
    RegisterClass(&wc);

    settingsHwnd = CreateWindow("SettingsWindow", "Settings", WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 300, 200, NULL, NULL, hInstance, NULL);

    CreateWindow("BUTTON", "Close", WS_VISIBLE | WS_CHILD, 50, 100, 80, 30, settingsHwnd, (HMENU)ID_BTN_CLOSE, hInstance, NULL);
    CreateWindow("BUTTON", "Minimize", WS_VISIBLE | WS_CHILD, 150, 100, 80, 30, settingsHwnd, (HMENU)ID_BTN_MINIMIZE, hInstance, NULL);
}

// Главная функция
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    WNDCLASS mwc = {};
    mwc.lpfnWndProc = MsgWndProc;
    mwc.hInstance = hInstance;
    mwc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);
    mwc.lpszClassName = "MyMsgBox";
    RegisterClass(&mwc);

    WNDCLASS wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = "ColorAnalyzer";
    RegisterClass(&wc);

    hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, "ColorAnalyzer", "ColorAnalyzer", WS_POPUP,
                          100, 100, 120, 50, NULL, NULL, hInstance, NULL);
    ShowWindow(hwnd, nCmdShow);
    addTrayIcon(hwnd);
    SetTimer(hwnd, IDT_UPDATE_COLOR, 0, NULL); // обновлять цвет каждую секунду
    RegisterHotKey(hwnd, ID_HOTKEY_SAVE, saveColorHotkey, saveColorKey);

    createSettingsWindow(hInstance);

    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);
    return 0;
}