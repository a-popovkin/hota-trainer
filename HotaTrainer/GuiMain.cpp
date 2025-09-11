#include <windows.h>
#include <commctrl.h>
#include <cmath>
#include <string>
#include <memory>

#include "Resource.h"
#include "Trainer.h"

// Link with ComCtl32.lib for trackbar controls
#pragma comment(lib, "comctl32.lib")

// Control IDs
#define ID_GOLD_CHECKBOX        1001
#define ID_GOLD_SLIDER          1002
#define ID_GOLD_VALUE_DISPLAY   1003
#define ID_MOVEMENT_CHECKBOX    1004
#define ID_MOVEMENT_SLIDER      1005
#define ID_MOVEMENT_VALUE_DISPLAY 1006
#define ID_GOLD_FREEZE_CHECKBOX        1007
#define ID_MOVEMENT_FREEZE_CHECKBOX    1008


#define ID_CUSTOM_CLOSE 2001

// Slider constants
#define SLIDER_MIN_POS    0
#define SLIDER_MAX_POS    1000    // High resolution for smooth logarithmic scaling
#define VALUE_MIN         1.0
#define VALUE_MAX         20.0

// Global Variables
HINSTANCE hInst;
HBITMAP hBitmapBackground = nullptr;
HBITMAP hBitmapClose = nullptr;
HFONT hFont = nullptr;  
HBITMAP hCtlBackground = nullptr; 
HBRUSH  hCtlBackgroundBrush = nullptr;

HWND hGoldCheckbox, hGoldSlider, hGoldValueDisplay;
HWND hMovementCheckbox, hMovementSlider, hMovementValueDisplay;
HWND hGoldFreezeCheckbox, hMovementFreezeCheckbox;


// Forward declarations
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
void CreateControls(HWND hWnd);
double SliderPosToValue(int position);
int ValueToSliderPos(double value);
void UpdateValueDisplay(HWND hSlider, HWND hDisplay);

void UpdateGoldSettings(Trainer* pTrainer);
void UpdateMovementSettings(Trainer* pTrainer);
double CustomRound(double value);

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    hInst = hInstance;

    // Initialize common controls
    InitCommonControls();

    // Register window class
    WNDCLASSEX wcex = { 0 };
    wcex.cbSize = sizeof(WNDCLASSEX);
    wcex.style = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc = WndProc;
    wcex.hInstance = hInstance;
    wcex.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wcex.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wcex.lpszClassName = L"MainWindow";

    RegisterClassEx(&wcex);

    // Injector entrance
    auto trainer = std::make_unique<Trainer>();

    if (!trainer->Start())
        return EXIT_FAILURE;

    hBitmapBackground = LoadBitmap(hInst, MAKEINTRESOURCEW(IDB_BITMAP1));
    hBitmapClose = LoadBitmap(hInst, MAKEINTRESOURCEW(IDB_BITMAP2));
    hCtlBackground = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP3));
    hCtlBackgroundBrush = CreatePatternBrush(hCtlBackground);

    // Create main window
    HWND hWnd = CreateWindow(
        L"MainWindow",
        L"HoTA trainer",
        WS_POPUP,
        CW_USEDEFAULT, CW_USEDEFAULT,
        512, 512,
        nullptr, nullptr, hInstance, trainer.get()
    );

    if (!hWnd)
        return FALSE;

    // Make window centered
    int screenWidth = GetSystemMetrics(SM_CXSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYSCREEN);

    RECT rc;
    GetWindowRect(hWnd, &rc);
    int wndWidth = rc.right - rc.left;
    int wndHeight = rc.bottom - rc.top;

    int x = (screenWidth - wndWidth) / 2;
    int y = (screenHeight - wndHeight) / 2;

    SetWindowPos(hWnd, nullptr, x, y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

    ShowWindow(hWnd, nCmdShow);
    UpdateWindow(hWnd);

    // Message loop
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    trainer->Stop();

    return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    static Trainer* pTrainer = nullptr;

    if (message == WM_NCCREATE)
    {
        CREATESTRUCT* pcs = reinterpret_cast<CREATESTRUCT*>(lParam);
        pTrainer = reinterpret_cast<Trainer*>(pcs->lpCreateParams);
        SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pTrainer));
    }
    else if (message != WM_NCDESTROY)
    {
        if (!pTrainer)
            pTrainer = reinterpret_cast<Trainer*>(GetWindowLongPtr(hWnd, GWLP_USERDATA));
    }

    switch (message)
    {
    case WM_CREATE:
        CreateControls(hWnd);
        break;

    case WM_COMMAND:
    {
        int wmId = LOWORD(wParam);
        int wmEvent = HIWORD(wParam);

        switch (wmId)
        {
        case ID_CUSTOM_CLOSE:
            DestroyWindow(hWnd);
            break;

        case ID_GOLD_CHECKBOX:
            if (wmEvent == BN_CLICKED)
                UpdateGoldSettings(pTrainer);

            break;

        case ID_MOVEMENT_CHECKBOX:
            if (wmEvent == BN_CLICKED)
                UpdateMovementSettings(pTrainer);

            break;
        case ID_GOLD_FREEZE_CHECKBOX:
            if (wmEvent == BN_CLICKED)
                UpdateGoldSettings(pTrainer);

            break;

        case ID_MOVEMENT_FREEZE_CHECKBOX:
            if (wmEvent == BN_CLICKED)
                UpdateMovementSettings(pTrainer);

            break;
        }
        break;
    }

    case WM_HSCROLL:
    {
        HWND hSlider = (HWND)lParam;

        if (hSlider == hGoldSlider)
        {
            UpdateValueDisplay(hGoldSlider, hGoldValueDisplay);
            UpdateGoldSettings(pTrainer);
        }
        else if (hSlider == hMovementSlider)
        {
            UpdateValueDisplay(hMovementSlider, hMovementValueDisplay);
            UpdateMovementSettings(pTrainer);
        }
    }
    break;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hWnd, &ps);

        if (hBitmapBackground)
        {
            HDC hdcMem = CreateCompatibleDC(hdc);
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hdcMem, hBitmapBackground);

            BITMAP bm;
            GetObject(hBitmapBackground, sizeof(bm), &bm);

            BitBlt(hdc, 0, 0, bm.bmWidth, bm.bmHeight, hdcMem, 0, 0, SRCCOPY);

            SelectObject(hdcMem, hOldBitmap);
            DeleteDC(hdcMem);
        }

        EndPaint(hWnd, &ps);
    }
    break;

    case WM_NCHITTEST:
    {
        LRESULT hit = DefWindowProc(hWnd, message, wParam, lParam);
        if (hit == HTCLIENT)
            return HTCAPTION;
        else
            return hit;
    }
    case WM_CTLCOLORSTATIC:
    case WM_CTLCOLORBTN:
    {
        HDC  hdc = (HDC)wParam;
        HWND hCtl = (HWND)lParam;

        SetTextColor(hdc, RGB(255, 251, 230));
        SetBkMode(hdc, TRANSPARENT); 
        if (hCtlBackgroundBrush)
        {
            POINT pt{ 0,0 };
            MapWindowPoints(hCtl, hWnd, &pt, 1);   
            SetBrushOrgEx(hdc, -pt.x, -pt.y, nullptr);
            return (INT_PTR)hCtlBackgroundBrush;        
        }
        return (INT_PTR)GetStockObject(HOLLOW_BRUSH);
    }

    case WM_DESTROY:
        if (hBitmapBackground)
            DeleteObject(hBitmapBackground);

        if (hBitmapClose)
            DeleteObject(hBitmapClose);

        if (hFont)
            DeleteObject(hFont);

        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void CreateControls(HWND hWnd)
{
    std::vector<HWND> controls;

    HWND hButton = CreateWindow(L"BUTTON", L"",
        WS_CHILD | WS_VISIBLE | BS_BITMAP,
        477, 5, 30, 30,
        hWnd, (HMENU)ID_CUSTOM_CLOSE, hInst, nullptr);
    SendMessage(hButton, BM_SETIMAGE, IMAGE_BITMAP, (LPARAM)hBitmapClose);
    controls.push_back(hButton);

    HWND hVersionLabel = CreateWindow(L"STATIC", L"HotA v1.7.3",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        425, 76, 80, 20,
        hWnd, nullptr, hInst, nullptr);
    controls.push_back(hVersionLabel);

    int baseY = 270; // bottom part

    HWND hGoldLabel = CreateWindow(L"STATIC", L"Gold:",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        20, baseY + 0, 100, 20,
        hWnd, nullptr, hInst, nullptr);
    controls.push_back(hGoldLabel);

    hGoldCheckbox = CreateWindow(L"BUTTON", L"Enable Gold",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        20, baseY + 25, 120, 20,
        hWnd, (HMENU)ID_GOLD_CHECKBOX, hInst, nullptr);
    controls.push_back(hGoldCheckbox);

    hGoldFreezeCheckbox = CreateWindow(
        L"BUTTON", L"Freeze gold",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        150, baseY + 25, 140, 20,
        hWnd, (HMENU)ID_GOLD_FREEZE_CHECKBOX, hInst, nullptr);
    controls.push_back(hGoldFreezeCheckbox);
    EnableWindow(hGoldFreezeCheckbox, FALSE); // disabled until gold is switched on

    hGoldSlider = CreateWindow(TRACKBAR_CLASS, L"",
        WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
        20, baseY + 55, 250, 30,
        hWnd, (HMENU)ID_GOLD_SLIDER, hInst, nullptr);
    controls.push_back(hGoldSlider);

    hGoldValueDisplay = CreateWindow(L"STATIC", L"1.0",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        280, baseY + 60, 30, 20,
        hWnd, (HMENU)ID_GOLD_VALUE_DISPLAY, hInst, nullptr);
    controls.push_back(hGoldValueDisplay);

    HWND goldScale1 = CreateWindow(L"STATIC", L"1.0",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        20, baseY + 90, 30, 20,
        hWnd, nullptr, hInst, nullptr);
    controls.push_back(goldScale1);

    HWND goldScale20 = CreateWindow(L"STATIC", L"20",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        240, baseY + 90, 30, 20,
        hWnd, nullptr, hInst, nullptr);
    controls.push_back(goldScale20);

    HWND hMovementLabel = CreateWindow(L"STATIC", L"Movement:",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        20, baseY + 130, 100, 20,
        hWnd, nullptr, hInst, nullptr);
    controls.push_back(hMovementLabel);

    hMovementCheckbox = CreateWindow(L"BUTTON", L"Enable Movement",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        20, baseY + 155, 140, 20,
        hWnd, (HMENU)ID_MOVEMENT_CHECKBOX, hInst, nullptr);
    controls.push_back(hMovementCheckbox);

    hMovementFreezeCheckbox = CreateWindow(
        L"BUTTON", L"Freeze movement",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        170, baseY + 155, 160, 20, 
        hWnd, (HMENU)ID_MOVEMENT_FREEZE_CHECKBOX, hInst, nullptr);
    controls.push_back(hMovementFreezeCheckbox);
    EnableWindow(hMovementFreezeCheckbox, FALSE); // disabled until movement is switched on

    hMovementSlider = CreateWindow(TRACKBAR_CLASS, L"",
        WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
        20, baseY + 185, 250, 30,
        hWnd, (HMENU)ID_MOVEMENT_SLIDER, hInst, nullptr);
    controls.push_back(hMovementSlider);

    hMovementValueDisplay = CreateWindow(L"STATIC", L"1.0",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        280, baseY + 190, 30, 20,
        hWnd, (HMENU)ID_MOVEMENT_VALUE_DISPLAY, hInst, nullptr);
    controls.push_back(hMovementValueDisplay);

    HWND movementScale1 = CreateWindow(L"STATIC", L"1.0",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        20, baseY + 220, 30, 20,
        hWnd, nullptr, hInst, nullptr);
    controls.push_back(movementScale1);

    HWND movementScale20 = CreateWindow(L"STATIC", L"20",
        WS_VISIBLE | WS_CHILD | SS_CENTER,
        240, baseY + 220, 30, 20,
        hWnd, nullptr, hInst, nullptr);
    controls.push_back(movementScale20);

    // Initial settings
    SendMessage(hGoldSlider, TBM_SETRANGE, TRUE, MAKELPARAM(SLIDER_MIN_POS, SLIDER_MAX_POS));
    SendMessage(hGoldSlider, TBM_SETPOS, TRUE, ValueToSliderPos(1.0));
    SendMessage(hGoldSlider, TBM_SETTICFREQ, SLIDER_MAX_POS / 10, 0);

    SendMessage(hMovementSlider, TBM_SETRANGE, TRUE, MAKELPARAM(SLIDER_MIN_POS, SLIDER_MAX_POS));
    SendMessage(hMovementSlider, TBM_SETPOS, TRUE, ValueToSliderPos(1.0));
    SendMessage(hMovementSlider, TBM_SETTICFREQ, SLIDER_MAX_POS / 10, 0);

    if (!hFont)
    {
        hFont = CreateFont(
            16, 0, 0, 0,
            FW_BOLD, FALSE, FALSE, FALSE,
            DEFAULT_CHARSET, OUT_DEFAULT_PRECIS,
            CLIP_DEFAULT_PRECIS, ANTIALIASED_QUALITY,
            FF_DONTCARE | DEFAULT_PITCH,
            L"Goudy Old Style"
        );
    }

    for (auto ctrl : controls)
        SendMessage(ctrl, WM_SETFONT, (WPARAM)hFont, TRUE);
}

// Convert slider position to logarithmic value (1.0 to 20.0)
double SliderPosToValue(int position)
{
    if (position <= SLIDER_MIN_POS) return VALUE_MIN;
    if (position >= SLIDER_MAX_POS) return VALUE_MAX;

    double ratio = (double)position / SLIDER_MAX_POS;
    return VALUE_MIN * pow(VALUE_MAX / VALUE_MIN, ratio);
}

// Convert logarithmic value to slider position
int ValueToSliderPos(double value)
{
    if (value <= VALUE_MIN) return SLIDER_MIN_POS;
    if (value >= VALUE_MAX) return SLIDER_MAX_POS;

    double ratio = log(value / VALUE_MIN) / log(VALUE_MAX / VALUE_MIN);
    return (int)(ratio * SLIDER_MAX_POS);
}

// Update value display for a slider
void UpdateValueDisplay(HWND hSlider, HWND hDisplay)
{
    int pos = static_cast<int>(SendMessage(hSlider, TBM_GETPOS, 0, 0));
    double rawValue = SliderPosToValue(pos);

    double roundedValue = CustomRound(rawValue);

    wchar_t buffer[32];
    if (roundedValue < 3.0)
        swprintf_s(buffer, L"%.1f", roundedValue);
    else
        swprintf_s(buffer, L"%.0f", roundedValue);
    SetWindowText(hDisplay, buffer);

    int correctPos = ValueToSliderPos(roundedValue);
    if (pos != correctPos)
    {
        SendMessage(hSlider, TBM_SETPOS, TRUE, correctPos);
    }
}

void UpdateGoldSettings(Trainer* pTrainer)
{
    bool isGoldEnabled = BST_CHECKED == SendMessage(hGoldCheckbox, BM_GETCHECK, 0, 0);
    bool freezeGold = BST_CHECKED == SendMessage(hGoldFreezeCheckbox, BM_GETCHECK, 0, 0);

    EnableWindow(hGoldSlider, !freezeGold);
    EnableWindow(hGoldFreezeCheckbox, isGoldEnabled);

    if (isGoldEnabled)
    {
        // Get current slider position and convert to logarithmic value
        int sliderPos = static_cast<int>(SendMessage(hGoldSlider, TBM_GETPOS, 0, 0));
        double goldValue = SliderPosToValue(sliderPos);

        pTrainer->UpdateGoldMultiplier(CustomRound(goldValue), freezeGold);
    }
    else
    {
        pTrainer->UpdateGoldMultiplier(1.0, false);
    }
}

void UpdateMovementSettings(Trainer* pTrainer)
{
    bool isMovementEnabled = BST_CHECKED == SendMessage(hMovementCheckbox, BM_GETCHECK, 0, 0);
    bool freezeMovement = BST_CHECKED == SendMessage(hMovementFreezeCheckbox, BM_GETCHECK, 0, 0);

    EnableWindow(hMovementSlider, !freezeMovement);
    EnableWindow(hMovementFreezeCheckbox, isMovementEnabled);

    if (isMovementEnabled)
    {
        // Get current slider position and convert to logarithmic value
        int sliderPos = static_cast<int>(SendMessage(hMovementSlider, TBM_GETPOS, 0, 0));
        double movementValue = SliderPosToValue(sliderPos);

        pTrainer->UpdateMovementMultiplier(CustomRound(movementValue), freezeMovement);
    }
    else
    {
        pTrainer->UpdateMovementMultiplier(1.0, false);
    }
}

double CustomRound(double value) 
{
    if (value < 3.0) 
        return std::round(value * 10.0) / 10.0;
    else 
        return std::round(value);
    
}