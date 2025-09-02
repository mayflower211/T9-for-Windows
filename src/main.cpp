// src/main.cpp
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <string>
#include <vector>
#include "T9Engine.h"
#include "resource.h"

// --- ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ ---
HHOOK g_hook = NULL;
HWND g_mainWindow = NULL;
HINSTANCE g_hInstance = NULL;
std::wstring g_currentSequence = L""; 
std::vector<std::wstring> g_suggestions;
T9Engine g_t9Engine;
T9Language g_currentLang = T9Language::English;
int g_selectedSuggestion = -1;
bool g_isServiceActive = true;
bool g_isDialogActive = false; // <-- НОВЫЙ ФЛАГ-ПАУЗА!

// --- ПРОТОТИПЫ ---
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
INT_PTR CALLBACK AddWordDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam);
void ShowAddWordDialog();


// --- ВСПОМОГАТЕЛЬНЫЕ ФУНКЦИИ ---
T9Language GetCurrentKeyboardLanguage() {
    HWND foreground = GetForegroundWindow();
    if (foreground) {
        DWORD threadId = GetWindowThreadProcessId(foreground, NULL);
        HKL layout = GetKeyboardLayout(threadId);
        if (LOWORD(layout) == 0x0419) return T9Language::Russian;
    }
    return T9Language::English;
}
void TypeText(const std::wstring& text) {
    std::vector<INPUT> inputs;
    for (wchar_t ch : text) {
        INPUT ip = {}; ip.type = INPUT_KEYBOARD; ip.ki.wScan = ch; ip.ki.dwFlags = KEYEVENTF_UNICODE;
        inputs.push_back(ip);
        ip.ki.dwFlags |= KEYEVENTF_KEYUP;
        inputs.push_back(ip);
    }
    SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
}


// --- ГЛАВНАЯ ФУНКЦИЯ ---
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    g_hInstance = hInstance;
    g_t9Engine.loadDictionaries(
        "dictionary_ru.txt", "user_dictionary_ru.txt",
        "dictionary_en.txt", "user_dictionary_en.txt"
    );

    const wchar_t CLASS_NAME[] = L"T9_Main_Window_Class";
    WNDCLASSEXW wc = {};
    wc.cbSize = sizeof(WNDCLASSEXW);
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = CLASS_NAME;
    wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    RegisterClassExW(&wc);

    g_mainWindow = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_TOOLWINDOW, 
        CLASS_NAME, L"T9 [ON]", 
        WS_POPUP | WS_BORDER, 
        CW_USEDEFAULT, CW_USEDEFAULT, 450, 250,
        NULL, NULL, hInstance, NULL );
    
    ShowWindow(g_mainWindow, nCmdShow);
    UpdateWindow(g_mainWindow);

    g_hook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, hInstance, 0);
    MSG msg = {};
    while (GetMessage(&msg, NULL, 0, 0) > 0)
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    UnhookWindowsHookEx(g_hook);
    return 0;
}


// --- ФУНКЦИЯ ДЛЯ ПОКАЗА ДИАЛОГА (ИЗМЕНЕНА: ИСПОЛЬЗУЕТ ФЛАГ-ПАУЗУ) ---
void ShowAddWordDialog() {
    // 1. Включаем флаг-паузу
    g_isDialogActive = true;

    DialogBoxParamW(g_hInstance, MAKEINTRESOURCE(IDD_ADDWORD_DIALOG), g_mainWindow, AddWordDialogProc, 0);

    // 2. После закрытия диалога выключаем флаг-паузу
    g_isDialogActive = false;
}


// --- ОБРАБОТЧИК ДИАЛОГОВОГО ОКНА (без изменений) ---
INT_PTR CALLBACK AddWordDialogProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    switch (message) {
        case WM_INITDIALOG: {
            SetFocus(GetDlgItem(hDlg, IDC_WORD_EDIT));
            std::wstring title = L"Добавить слово для: " + g_currentSequence;
            SetWindowTextW(hDlg, title.c_str());
            return (INT_PTR)TRUE;
        }
        case WM_COMMAND:
            if (LOWORD(wParam) == IDOK) {
                wchar_t newWord[100] = {0};
                GetDlgItemTextW(hDlg, IDC_WORD_EDIT, newWord, 100);
                if (wcslen(newWord) > 0) {
                    g_t9Engine.addWord(newWord, g_currentLang);
                }
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            if (LOWORD(wParam) == IDCANCEL) {
                EndDialog(hDlg, LOWORD(wParam));
                return (INT_PTR)TRUE;
            }
            break;
    }
    return (INT_PTR)FALSE;
}


// --- ПРОЦЕДУРА ОКНА (ОТРИСОВКА) (без изменений) ---
LRESULT CALLBACK WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_DESTROY: PostQuitMessage(0); return 0;
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        FillRect(hdc, &ps.rcPaint, (HBRUSH)(COLOR_WINDOW + 1));
        
        RECT rect; GetClientRect(hwnd, &rect);
        SetBkMode(hdc, TRANSPARENT);

        HFONT hFontLarge = CreateFontW(36, 0, 0, 0, FW_BOLD, 0,0,0, DEFAULT_CHARSET, 0,0,0,0, L"Consolas");
        HFONT hOldFont = (HFONT)SelectObject(hdc, hFontLarge);
        std::wstring lang_indicator = (g_currentLang == T9Language::Russian) ? L"[RU]" : L"[EN]";
        std::wstring topText = (g_currentSequence.empty() ? L"..." : g_currentSequence) + L" " + lang_indicator;
        DrawTextW(hdc, topText.c_str(), -1, &rect, DT_CENTER | DT_TOP | DT_SINGLELINE);

        HFONT hFontMid = CreateFontW(22, 0, 0, 0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0,0,0, L"Arial");
        SelectObject(hdc, hFontMid);
        std::wstring suggestionsText;
        if (g_isServiceActive) {
            if (!g_suggestions.empty()){
                for(size_t i = 0; i < g_suggestions.size(); ++i) {
                    if (i == g_selectedSuggestion) suggestionsText += L"[" + g_suggestions[i] + L"] ";
                    else suggestionsText += g_suggestions[i] + L" ";
                }
            } else if (!g_currentSequence.empty()) {
                suggestionsText = L"Нет совпадений. [+] - добавить";
            }
        } else {
            suggestionsText = L"Сервис отключен. Нажмите Scroll Lock";
        }
        rect.top += 50; 
        DrawTextW(hdc, suggestionsText.c_str(), -1, &rect, DT_CENTER | DT_TOP | DT_WORDBREAK);

        HFONT hFontHelp = CreateFontW(16, 0, 0, 0, FW_NORMAL, 0,0,0, DEFAULT_CHARSET, 0,0,0,0, L"Consolas");
        SelectObject(hdc, hFontHelp);
        const wchar_t* help_text_ru = L"2:абвг 3:дежз 4:ийкл 5:мноп\n6:рсту 7:фхцч 8:шщъы 9:ьэюя";
        const wchar_t* help_text_en = L"2:abc 3:def 4:ghi 5:jkl\n6:mno 7:pqrs 8:tuv 9:wxyz";
        const wchar_t* help_to_draw = (g_currentLang == T9Language::Russian) ? help_text_ru : help_text_en;
        rect.top += 70;
        DrawTextW(hdc, help_to_draw, -1, &rect, DT_CENTER | DT_TOP);

        SelectObject(hdc, hOldFont); 
        DeleteObject(hFontLarge); 
        DeleteObject(hFontMid);
        DeleteObject(hFontHelp);
        EndPaint(hwnd, &ps);
    } return 0;
    }
    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}


// --- ПЕРЕХВАТЧИК КЛАВИАТУРЫ (ИЗМЕНЕН: ИСПОЛЬЗУЕТ ФЛАГ-ПАУЗУ) ---
LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    // --- ГЛАВНАЯ ПРОВЕРКА НА ПАУЗУ ---
    // Если поднят флаг (показывается диалог), хук ничего не делает и пропускает все нажатия.
    if (g_isDialogActive) {
        return CallNextHookEx(g_hook, nCode, wParam, lParam);
    }
    
    if (nCode == HC_ACTION && wParam == WM_KEYDOWN)
    {
        KBDLLHOOKSTRUCT* pkbhs = (KBDLLHOOKSTRUCT*)lParam;
        
        switch(pkbhs->vkCode) {
            case VK_SCROLL:
                g_isServiceActive = !g_isServiceActive;
                SetWindowTextW(g_mainWindow, g_isServiceActive ? L"T9 [ON]" : L"T9 [OFF]");
                InvalidateRect(g_mainWindow, NULL, TRUE);
                return 1;
            case VK_PAUSE:
                g_currentLang = (g_currentLang == T9Language::Russian) ? T9Language::English : T9Language::Russian;
                InvalidateRect(g_mainWindow, NULL, TRUE);
                return 1;
        }

        if (!g_isServiceActive) {
            return CallNextHookEx(g_hook, nCode, wParam, lParam);
        }

        bool shouldUpdate = false; 
        switch(pkbhs->vkCode) {
            case VK_NUMPAD0: case VK_NUMPAD1: case VK_NUMPAD2: case VK_NUMPAD3: case VK_NUMPAD4:
            case VK_NUMPAD5: case VK_NUMPAD6: case VK_NUMPAD7: case VK_NUMPAD8: case VK_NUMPAD9:
                g_currentSequence += std::to_wstring(pkbhs->vkCode - VK_NUMPAD0);
                shouldUpdate = true;
                break;
            case VK_BACK:
                if (!g_currentSequence.empty()) g_currentSequence.pop_back();
                shouldUpdate = true;
                break;
            case VK_LEFT:
                if (!g_suggestions.empty()) {
                    g_selectedSuggestion = (g_selectedSuggestion <= 0) ? (int)g_suggestions.size() - 1 : g_selectedSuggestion - 1;
                    InvalidateRect(g_mainWindow, NULL, FALSE);
                } return 1;
            case VK_RIGHT:
                if (!g_suggestions.empty()) {
                    g_selectedSuggestion = (g_selectedSuggestion + 1) % g_suggestions.size();
                    InvalidateRect(g_mainWindow, NULL, FALSE);
                } return 1;
            case VK_SPACE: case VK_RETURN:
                if (g_selectedSuggestion != -1 && g_selectedSuggestion < (int)g_suggestions.size()) {
                    TypeText(g_suggestions[g_selectedSuggestion]);
                    if (pkbhs->vkCode == VK_SPACE) TypeText(L" ");
                    g_currentSequence.clear();
                    shouldUpdate = true;
                } return 1;
            case VK_DIVIDE:
                g_t9Engine.loadDictionaries(
                    "dictionary_ru.txt", "user_dictionary_ru.txt",
                    "dictionary_en.txt", "user_dictionary_en.txt"
                );
                g_currentSequence.clear();
                shouldUpdate = true;
                break;
            case VK_ADD:
                if (!g_currentSequence.empty()) {
                    ShowAddWordDialog();
                    g_currentSequence.clear();
                    shouldUpdate = true;
                }
                 return 1;
        }

        if (shouldUpdate) {
            g_currentLang = GetCurrentKeyboardLanguage();
            g_suggestions = g_t9Engine.getSuggestions(g_currentSequence, g_currentLang);
            g_selectedSuggestion = g_suggestions.empty() ? -1 : 0;
            InvalidateRect(g_mainWindow, NULL, TRUE);
            return 1;
        }
    }
    return CallNextHookEx(g_hook, nCode, wParam, lParam);
}