#include <windows.h>
#include <commctrl.h>
#include <cmath>
#include <string>
#include <memory>

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

// Slider constants
#define SLIDER_MIN_POS    0
#define SLIDER_MAX_POS    1000    // High resolution for smooth logarithmic scaling
#define VALUE_MIN         1.0
#define VALUE_MAX         20.0

// Global Variables
HINSTANCE hInst;
HWND hGoldCheckbox, hGoldSlider, hGoldValueDisplay;
HWND hMovementCheckbox, hMovementSlider, hMovementValueDisplay;

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

    // Create main window
    HWND hWnd = CreateWindow(
        L"MainWindow",
        L"HoTA trainer",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT,
        450, 320,
        nullptr, nullptr, hInstance, trainer.get()
    );

    if (!hWnd)
        return FALSE;

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
        case ID_GOLD_CHECKBOX:
            if (wmEvent == BN_CLICKED)
                UpdateGoldSettings(pTrainer);

            break;

        case ID_MOVEMENT_CHECKBOX:
            if (wmEvent == BN_CLICKED)
                UpdateMovementSettings(pTrainer);

            break;
        }
    }
    break;

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

    case WM_DESTROY:
        PostQuitMessage(0);
        break;

    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void CreateControls(HWND hWnd)
{
    // Gold section label
    CreateWindow(L"STATIC", L"Gold:",
        WS_VISIBLE | WS_CHILD,
        20, 20, 100, 20,
        hWnd, nullptr, hInst, nullptr);

    // Gold checkbox
    hGoldCheckbox = CreateWindow(L"BUTTON", L"Enable Gold",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        20, 45, 120, 25,
        hWnd, (HMENU)ID_GOLD_CHECKBOX, hInst, nullptr);

    // Gold slider
    hGoldSlider = CreateWindow(TRACKBAR_CLASS, L"",
        WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
        20, 75, 250, 30,
        hWnd, (HMENU)ID_GOLD_SLIDER, hInst, nullptr);

    // Configure gold slider
    SendMessage(hGoldSlider, TBM_SETRANGE, TRUE, MAKELPARAM(SLIDER_MIN_POS, SLIDER_MAX_POS));
    SendMessage(hGoldSlider, TBM_SETPOS, TRUE, ValueToSliderPos(1.0)); // Default to 1.0
    SendMessage(hGoldSlider, TBM_SETTICFREQ, SLIDER_MAX_POS / 10, 0);

    // Gold value display
    hGoldValueDisplay = CreateWindow(L"STATIC", L"1.0",
        WS_VISIBLE | WS_CHILD,
        280, 80, 60, 20,
        hWnd, (HMENU)ID_GOLD_VALUE_DISPLAY, hInst, nullptr);

    // Movement section label
    CreateWindow(L"STATIC", L"Movement:",
        WS_VISIBLE | WS_CHILD,
        20, 130, 100, 20,
        hWnd, nullptr, hInst, nullptr);

    // Movement checkbox
    hMovementCheckbox = CreateWindow(L"BUTTON", L"Enable Movement",
        WS_VISIBLE | WS_CHILD | BS_AUTOCHECKBOX,
        20, 155, 140, 25,
        hWnd, (HMENU)ID_MOVEMENT_CHECKBOX, hInst, nullptr);

    // Movement slider
    hMovementSlider = CreateWindow(TRACKBAR_CLASS, L"",
        WS_VISIBLE | WS_CHILD | TBS_HORZ | TBS_AUTOTICKS,
        20, 185, 250, 30,
        hWnd, (HMENU)ID_MOVEMENT_SLIDER, hInst, nullptr);

    // Configure movement slider
    SendMessage(hMovementSlider, TBM_SETRANGE, TRUE, MAKELPARAM(SLIDER_MIN_POS, SLIDER_MAX_POS));
    SendMessage(hMovementSlider, TBM_SETPOS, TRUE, ValueToSliderPos(1.0)); // Default to 1.0
    SendMessage(hMovementSlider, TBM_SETTICFREQ, SLIDER_MAX_POS / 10, 0);

    // Movement value display
    hMovementValueDisplay = CreateWindow(L"STATIC", L"1.0",
        WS_VISIBLE | WS_CHILD,
        280, 190, 60, 20,
        hWnd, (HMENU)ID_MOVEMENT_VALUE_DISPLAY, hInst, nullptr);

    // Scale labels
    CreateWindow(L"STATIC", L"1.0",
        WS_VISIBLE | WS_CHILD,
        20, 105, 30, 15,
        hWnd, nullptr, hInst, nullptr);

    CreateWindow(L"STATIC", L"20",
        WS_VISIBLE | WS_CHILD,
        240, 105, 30, 15,
        hWnd, nullptr, hInst, nullptr);

    CreateWindow(L"STATIC", L"1.0",
        WS_VISIBLE | WS_CHILD,
        20, 215, 30, 15,
        hWnd, nullptr, hInst, nullptr);

    CreateWindow(L"STATIC", L"20",
        WS_VISIBLE | WS_CHILD,
        240, 215, 30, 15,
        hWnd, nullptr, hInst, nullptr);
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
    bool isGoldEnabled = (BST_CHECKED == SendMessage(hGoldCheckbox, BM_GETCHECK, 0, 0));

    if (isGoldEnabled)
    {
        // Get current slider position and convert to logarithmic value
        int sliderPos = static_cast<int>(SendMessage(hGoldSlider, TBM_GETPOS, 0, 0));
        double goldValue = SliderPosToValue(sliderPos);

        pTrainer->UpdateGoldMultiplier(CustomRound(goldValue));
    }
    else
    {
        pTrainer->UpdateGoldMultiplier(1.0);
    }
}

void UpdateMovementSettings(Trainer* pTrainer)
{
    bool isMovementEnabled = (BST_CHECKED == SendMessage(hMovementCheckbox, BM_GETCHECK, 0, 0));

    if (isMovementEnabled)
    {
        // Get current slider position and convert to logarithmic value
        int sliderPos = static_cast<int>(SendMessage(hMovementSlider, TBM_GETPOS, 0, 0));
        double movementValue = SliderPosToValue(sliderPos);

        pTrainer->UpdateMovementMultiplier(CustomRound(movementValue));
    }
    else
    {
        pTrainer->UpdateMovementMultiplier(1.0);
    }
}

double CustomRound(double value) 
{
    if (value < 3.0) 
        return std::round(value * 10.0) / 10.0;
    else 
        return std::round(value);
    
}