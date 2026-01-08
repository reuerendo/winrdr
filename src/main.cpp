#include <windows.h>
#include <commdlg.h>
#include <string>
#include <vector>
#include "epub/epub_parser.h"
#include "utils/logger.h"

// Глобальные переменные
epub::EpubParser g_parser;
std::wstring g_current_text;
size_t g_current_chapter = 0;
HWND g_hwnd_main = nullptr;

// Конвертация UTF-8 в Wide String
std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

// Конвертация Wide String в UTF-8
std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

// Загрузка главы
void LoadChapter(size_t index) {
    if (index >= g_parser.getChapterCount()) {
        LOG_WARNING("Invalid chapter index:", index);
        return;
    }
    
    LOG_INFO("Loading chapter:", index + 1, "/", g_parser.getChapterCount());
    
    g_current_chapter = index;
    std::string text = g_parser.getChapterText(index);
    g_current_text = utf8_to_wstring(text);
    
    LOG_DEBUG("Chapter text length:", text.length(), "bytes");
    
    if (g_hwnd_main) {
        InvalidateRect(g_hwnd_main, nullptr, TRUE);
        
        // Обновляем заголовок окна
        auto meta = g_parser.getMetadata();
        std::wstring title = utf8_to_wstring(meta.title) + 
                            L" - Глава " + std::to_wstring(index + 1) + 
                            L" / " + std::to_wstring(g_parser.getChapterCount());
        SetWindowTextW(g_hwnd_main, title.c_str());
    }
}

// Открытие файла
void OpenFile() {
    LOG_INFO("Opening file dialog");
    
    OPENFILENAMEW ofn = {};
    wchar_t filename[MAX_PATH] = {};
    
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = g_hwnd_main;
    ofn.lpstrFilter = L"EPUB Files\0*.epub\0All Files\0*.*\0";
    ofn.lpstrFile = filename;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
    
    if (GetOpenFileNameW(&ofn)) {
        std::string path = wstring_to_utf8(filename);
        LOG_INFO("Selected file:", path);
        
        if (g_parser.open(path)) {
            LOG_INFO("EPUB file opened successfully");
            LoadChapter(0);
        } else {
            LOG_ERROR("Failed to open EPUB file:", path);
            MessageBoxW(g_hwnd_main, L"Не удалось открыть EPUB файл", L"Ошибка", MB_OK | MB_ICONERROR);
        }
    } else {
        LOG_DEBUG("File dialog cancelled");
    }
}

// Следующая глава
void NextChapter() {
    if (g_current_chapter + 1 < g_parser.getChapterCount()) {
        LoadChapter(g_current_chapter + 1);
    }
}

// Предыдущая глава
void PrevChapter() {
    if (g_current_chapter > 0) {
        LoadChapter(g_current_chapter - 1);
    }
}

// Обработчик сообщений окна
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE:
            g_hwnd_main = hwnd;
            break;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            // Белый фон
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
            
            // Рисуем текст
            if (!g_current_text.empty()) {
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(0, 0, 0));
                
                HFONT hfont = CreateFontW(20, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE,
                    DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                    DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Arial");
                HFONT old_font = (HFONT)SelectObject(hdc, hfont);
                
                rect.left += 20;
                rect.top += 20;
                rect.right -= 20;
                rect.bottom -= 20;
                
                DrawTextW(hdc, g_current_text.c_str(), -1, &rect, 
                         DT_LEFT | DT_TOP | DT_WORDBREAK);
                
                SelectObject(hdc, old_font);
                DeleteObject(hfont);
            } else {
                // Если книга не загружена
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(128, 128, 128));
                const wchar_t* msg = L"Нажмите Ctrl+O для открытия EPUB файла";
                DrawTextW(hdc, msg, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_KEYDOWN:
            switch (wparam) {
                case VK_RIGHT:
                case VK_NEXT: // Page Down
                    NextChapter();
                    break;
                case VK_LEFT:
                case VK_PRIOR: // Page Up
                    PrevChapter();
                    break;
                case 'O':
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        OpenFile();
                    }
                    break;
            }
            break;
            
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case 1: // Открыть
                    OpenFile();
                    break;
                case 2: // Выход
                    PostQuitMessage(0);
                    break;
            }
            break;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

// Главная функция
int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE, LPSTR, int cmdshow) {
    try {
        // Открываем консоль для отладки
        AllocConsole();
        FILE* dummy;
        freopen_s(&dummy, "CONOUT$", "w", stdout);
        freopen_s(&dummy, "CONOUT$", "w", stderr);
        
        // Инициализация логера
        Logger::instance().init("epub_reader.log", LOG_LEVEL_DEBUG);
        LOG_INFO("=== EPUB Reader started ===");
        
        printf("Application started\n");
        fflush(stdout);
    
    // Регистрация класса окна
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = L"EpubReaderClass";
    
    if (!RegisterClassW(&wc)) {
        LOG_ERROR("Failed to register window class");
        MessageBoxW(NULL, L"Failed to register window class", L"Error", MB_OK | MB_ICONERROR);
        return 1;
    }
    
    LOG_DEBUG("Window class registered");
    printf("Window class registered\n");
    
    // Создание меню
    HMENU menu = CreateMenu();
    HMENU file_menu = CreateMenu();
    AppendMenuW(file_menu, MF_STRING, 1, L"Открыть (Ctrl+O)");
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, 2, L"Выход");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)file_menu, L"Файл");
    
    // Создание окна
    HWND hwnd = CreateWindowExW(
        0,
        L"EpubReaderClass",
        L"EPUB Reader",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        800, 600,
        nullptr, menu, hinstance, nullptr
    );
    
    if (!hwnd) {
        LOG_ERROR("Failed to create window");
        DWORD error = GetLastError();
        wchar_t msg[256];
        swprintf_s(msg, L"Failed to create window. Error: %lu", error);
        MessageBoxW(NULL, msg, L"Error", MB_OK | MB_ICONERROR);
        printf("Failed to create window. Error: %lu\n", error);
        return 1;
    }
    
    LOG_INFO("Main window created");
    printf("Main window created\n");
    
    ShowWindow(hwnd, cmdshow);
    UpdateWindow(hwnd);
    
    LOG_INFO("Entering message loop");
    
    // Цикл сообщений
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    LOG_INFO("=== EPUB Reader exiting ===");
    
    return (int)msg.wParam;
    
    } catch (const std::exception& e) {
        LOG_ERROR("Exception:", e.what());
        MessageBoxA(NULL, e.what(), "Fatal Error", MB_OK | MB_ICONERROR);
        return 1;
    } catch (...) {
        LOG_ERROR("Unknown exception");
        MessageBoxA(NULL, "Unknown exception occurred", "Fatal Error", MB_OK | MB_ICONERROR);
        return 1;
    }
}