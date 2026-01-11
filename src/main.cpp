#include <windows.h>
#include <commdlg.h>
#include <commctrl.h>
#include <string>
#include <vector>
#include "epub/epub_parser.h"
#include "epub/reading_position.h"
#include "render/page_renderer.h"
#include "epub/css/css_box_model_applier.h"
#include "epub/css/css_types.h"
#include "utils/logger.h"

epub::EpubParser g_parser;
PageRenderer g_renderer;
size_t g_current_chapter = 0;
HWND g_hwnd_main = nullptr;
std::string g_current_file;
HWND g_toc_window = nullptr;
std::vector<size_t> g_toc_chapter_indices;

void testCSSParsing() {
    LOG_INFO("=== Testing CSS Box Model Parsing ===");
    
    epub::CSSBoxModelApplier applier;
    epub::CSSComputedStyle style;
    
    // Test 1: margin-top with rem
    applier.applyProperty("margin-top", "10.5rem", style);
    LOG_INFO("Test 1 - margin-top: 10.5rem ->", style.margin_top, "(expected: 168)");
    
    // Test 2: margin with shorthand
    style = epub::CSSComputedStyle();
    applier.applyProperty("margin", "1.5rem", style);
    LOG_INFO("Test 2 - margin: 1.5rem ->", 
             "top:", style.margin_top, 
             "right:", style.margin_right,
             "bottom:", style.margin_bottom,
             "left:", style.margin_left,
             "(expected: 24 24 24 24)");
    
    // Test 3: padding
    style = epub::CSSComputedStyle();
    applier.applyProperty("padding", "0 0rem 0 0rem", style);
    LOG_INFO("Test 3 - padding: 0 0rem 0 0rem ->",
             "top:", style.padding_top,
             "right:", style.padding_right,
             "bottom:", style.padding_bottom,
             "left:", style.padding_left,
             "(expected: 0 0 0 0)");
    
    // Test 4: text-indent
    style = epub::CSSComputedStyle();
    applier.applyProperty("text-indent", "1.5em", style);
    LOG_INFO("Test 4 - text-indent: 1.5em ->", style.text_indent, "(expected: 24)");
    
    LOG_INFO("=== CSS Parsing Tests Complete ===");
}

std::wstring GetExecutablePath() {
    wchar_t path[MAX_PATH];
    GetModuleFileNameW(NULL, path, MAX_PATH);
    std::wstring ws_path(path);
    size_t last_slash = ws_path.find_last_of(L"\\/");
    if (last_slash != std::wstring::npos) {
        return ws_path.substr(0, last_slash + 1);
    }
    return L"";
}

std::wstring utf8_to_wstring(const std::string& str) {
    if (str.empty()) return std::wstring();
    int size = MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str.c_str(), -1, &result[0], size);
    return result;
}

std::string wstring_to_utf8(const std::wstring& wstr) {
    if (wstr.empty()) return std::string();
    int size = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(size, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &result[0], size, nullptr, nullptr);
    return result;
}

void UpdateTitle() {
    if (!g_hwnd_main) return;
    
    auto meta = g_parser.getMetadata();
    std::wstring title = utf8_to_wstring(meta.title);
    
    if (g_parser.getChapterCount() > 0) {
        title += L" - Глава " + std::to_wstring(g_current_chapter + 1) + 
                 L" / " + std::to_wstring(g_parser.getChapterCount());
        
        // Only show page info if pages are calculated
        if (g_renderer.getPageCount() > 0) {
            title += L" - Страница " + std::to_wstring(g_renderer.getCurrentPage() + 1) +
                     L" / " + std::to_wstring(g_renderer.getPageCount());
        }
    } else {
        title = L"EPUB Reader";
    }
    
    SetWindowTextW(g_hwnd_main, title.c_str());
}

void SavePosition() {
    if (g_current_file.empty()) return;
    
    POSITION_MGR.savePosition(g_current_file, g_current_chapter, g_renderer.getCurrentPage());
    LOG_DEBUG("Position saved:", g_current_chapter, g_renderer.getCurrentPage());
}

void LoadChapter(size_t index) {
    if (index >= g_parser.getChapterCount()) {
        LOG_WARNING("Invalid chapter index:", index);
        return;
    }
    
    LOG_INFO("Loading chapter:", index + 1, "/", g_parser.getChapterCount());
    
    g_current_chapter = index;
    
    epub::FormattedContent content = g_parser.getChapterContent(index);
    
    LOG_DEBUG("Chapter content elements:", content.size());
    
    g_renderer.setContent(content);
    
    if (g_hwnd_main) {
        InvalidateRect(g_hwnd_main, nullptr, TRUE);
        UpdateWindow(g_hwnd_main);
        UpdateTitle();
    }
    
    // Don't save here - will be saved after goToPage or navigation
}

LRESULT CALLBACK TOCWindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_SIZE: {
            HWND listbox = GetDlgItem(hwnd, 1001);
            if (listbox) {
                RECT rect;
                GetClientRect(hwnd, &rect);
                SetWindowPos(listbox, NULL, 0, 0, rect.right, rect.bottom, 
                           SWP_NOZORDER | SWP_NOMOVE);
            }
            break;
        }
        
        case WM_KEYDOWN:
            if (wparam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            } else if (wparam == VK_RETURN) {
                HWND listbox = GetDlgItem(hwnd, 1001);
                int sel = (int)SendMessageW(listbox, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)g_toc_chapter_indices.size()) {
                    size_t chapter = g_toc_chapter_indices[sel];
                    DestroyWindow(hwnd);
                    LoadChapter(chapter);
                    SavePosition();
                }
                return 0;
            }
            break;
        
        case WM_COMMAND:
            if (LOWORD(wparam) == 1001 && HIWORD(wparam) == LBN_DBLCLK) {
                HWND listbox = GetDlgItem(hwnd, 1001);
                int sel = (int)SendMessageW(listbox, LB_GETCURSEL, 0, 0);
                if (sel >= 0 && sel < (int)g_toc_chapter_indices.size()) {
                    size_t chapter = g_toc_chapter_indices[sel];
                    DestroyWindow(hwnd);
                    LoadChapter(chapter);
                    SavePosition();
                }
            }
            break;
            
        case WM_CLOSE:
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            g_toc_window = nullptr;
            g_toc_chapter_indices.clear();
            EnableWindow(g_hwnd_main, TRUE);
            SetFocus(g_hwnd_main);
            return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

void ShowTOC() {
    const auto& toc = g_parser.getTOC();
    
    if (toc.empty()) {
        MessageBoxW(g_hwnd_main, L"Оглавление недоступно для этой книги", 
                   L"Оглавление", MB_OK | MB_ICONINFORMATION);
        return;
    }
    
    if (g_toc_window) {
        SetFocus(g_toc_window);
        return;
    }
    
    // Register window class for TOC
    static bool class_registered = false;
    if (!class_registered) {
        WNDCLASSW wc = {};
        wc.lpfnWndProc = TOCWindowProc;
        wc.hInstance = GetModuleHandle(NULL);
        wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
        wc.lpszClassName = L"TOCWindowClass";
        RegisterClassW(&wc);
        class_registered = true;
    }
    
    // Create TOC window
    g_toc_window = CreateWindowExW(
        WS_EX_DLGMODALFRAME | WS_EX_TOPMOST,
        L"TOCWindowClass",
        L"Оглавление",
        WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE,
        100, 100, 600, 400,
        g_hwnd_main, NULL, GetModuleHandle(NULL), NULL
    );
    
    if (!g_toc_window) return;
    
    // Create listbox inside TOC window
    HWND listbox = CreateWindowExW(
        0,
        WC_LISTBOXW,
        NULL,
        WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_NOTIFY | LBS_HASSTRINGS,
        0, 0, 600, 400,
        g_toc_window, (HMENU)1001, GetModuleHandle(NULL), NULL
    );
    
    if (!listbox) {
        DestroyWindow(g_toc_window);
        return;
    }
    
    // Set default GUI font for listbox
    SendMessageW(listbox, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), TRUE);
    
    // Add TOC items
    g_toc_chapter_indices.clear();
    for (const auto& item : toc) {
        std::wstring indent(item.level * 2, L' ');
        std::wstring text = indent + utf8_to_wstring(item.title);
        SendMessageW(listbox, LB_ADDSTRING, 0, (LPARAM)text.c_str());
        g_toc_chapter_indices.push_back(item.spine_index);
    }
    
    EnableWindow(g_hwnd_main, FALSE);
    ShowWindow(g_toc_window, SW_SHOW);
    SetFocus(listbox);
}

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
            
            // Load default EPUB styles
            std::wstring exe_path = GetExecutablePath();
            std::string default_css_path = wstring_to_utf8(exe_path + L"epub.css");
            g_parser.getHTMLProcessor().getCSSProcessor().loadDefaultStyles(default_css_path);
            
            g_current_file = path;
            g_renderer.setImageCache(&g_parser.getImageCache());
            
            // Try to restore position
            epub::BookPosition pos = POSITION_MGR.loadPosition(path);
            
            if (pos.chapter_index < g_parser.getChapterCount()) {
                LoadChapter(pos.chapter_index);
                g_renderer.goToPage(pos.page_index);
                InvalidateRect(g_hwnd_main, nullptr, TRUE);
                UpdateWindow(g_hwnd_main);
                UpdateTitle();
                SavePosition();
                LOG_INFO("Restored position:", pos.chapter_index, pos.page_index);
            } else {
                LoadChapter(0);
                SavePosition();
            }
        } else {
            LOG_ERROR("Failed to open EPUB file:", path);
            MessageBoxW(g_hwnd_main, L"Не удалось открыть EPUB файл", 
                       L"Ошибка", MB_OK | MB_ICONERROR);
        }
    } else {
        LOG_DEBUG("File dialog cancelled");
    }
}

void NextChapter() {
    if (g_current_chapter + 1 < g_parser.getChapterCount()) {
        LoadChapter(g_current_chapter + 1);
        SavePosition();
    }
}

void PrevChapter() {
    if (g_current_chapter > 0) {
        LoadChapter(g_current_chapter - 1);
        SavePosition();
    }
}

void NextPage() {
    if (g_renderer.nextPage()) {
        InvalidateRect(g_hwnd_main, nullptr, TRUE);
        UpdateTitle();
        SavePosition();
    } else {
        NextChapter();
    }
}

void PrevPage() {
    if (g_renderer.prevPage()) {
        InvalidateRect(g_hwnd_main, nullptr, TRUE);
        UpdateTitle();
        SavePosition();
    } else {
        if (g_current_chapter > 0) {
            PrevChapter();
            // Go to last page of previous chapter
            if (g_renderer.getPageCount() > 0) {
                g_renderer.goToPage(g_renderer.getPageCount() - 1);
                InvalidateRect(g_hwnd_main, nullptr, TRUE);
                UpdateTitle();
                SavePosition();
            }
        }
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    switch (msg) {
        case WM_CREATE:
            g_hwnd_main = hwnd;
            RECT rect;
            GetClientRect(hwnd, &rect);
            g_renderer.setViewport(rect.right, rect.bottom, 40);
            g_renderer.setFont(L"Arial", 18);
            break;
            
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hwnd, &ps);
            
            RECT rect;
            GetClientRect(hwnd, &rect);
            
            FillRect(hdc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
            
            if (g_parser.getChapterCount() > 0) {
                g_renderer.render(hdc);
            } else {
                SetBkMode(hdc, TRANSPARENT);
                SetTextColor(hdc, RGB(128, 128, 128));
                const wchar_t* msg = L"Нажмите Ctrl+O для открытия EPUB файла\n\n"
                                    L"Навигация:\n"
                                    L"→ / Page Down - следующая страница\n"
                                    L"← / Page Up - предыдущая страница\n"
                                    L"Ctrl+→ - следующая глава\n"
                                    L"Ctrl+← - предыдущая глава\n"
                                    L"Ctrl+T - оглавление";
                DrawTextW(hdc, msg, -1, &rect, DT_CENTER | DT_VCENTER);
            }
            
            EndPaint(hwnd, &ps);
            return 0;
        }
        
        case WM_KEYDOWN:
            switch (wparam) {
                case VK_RIGHT:
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        NextChapter();
                    } else {
                        NextPage();
                    }
                    break;
                case VK_LEFT:
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        PrevChapter();
                    } else {
                        PrevPage();
                    }
                    break;
                case VK_NEXT:
                    NextPage();
                    break;
                case VK_PRIOR:
                    PrevPage();
                    break;
                case 'O':
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        OpenFile();
                    }
                    break;
                case 'T':
                    if (GetKeyState(VK_CONTROL) & 0x8000) {
                        ShowTOC();
                    }
                    break;
            }
            break;
            
        case WM_SIZE: {
            RECT rect;
            GetClientRect(hwnd, &rect);
            g_renderer.setViewport(rect.right, rect.bottom, 40);
            InvalidateRect(hwnd, nullptr, TRUE);
            break;
        }
        
        case WM_COMMAND:
            switch (LOWORD(wparam)) {
                case 1:
                    OpenFile();
                    break;
                case 2:
                    ShowTOC();
                    break;
                case 3:
                    PostQuitMessage(0);
                    break;
            }
            break;
            
        case WM_CLOSE:
            SavePosition();
            DestroyWindow(hwnd);
            return 0;
            
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

int WINAPI WinMain(HINSTANCE hinstance, HINSTANCE, LPSTR, int cmdshow) {
    try {
        Logger::instance().init("epub_reader.log", LOG_LEVEL_DEBUG);
        LOG_INFO("=== EPUB Reader started ===");
        
        InitCommonControls();
    
    WNDCLASSW wc = {};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = hinstance;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszClassName = L"EpubReaderClass";
    
    if (!RegisterClassW(&wc)) {
        LOG_ERROR("Failed to register window class");
        return 1;
    }
    
    LOG_DEBUG("Window class registered");
    
    HMENU menu = CreateMenu();
    HMENU file_menu = CreateMenu();
    AppendMenuW(file_menu, MF_STRING, 1, L"Открыть (Ctrl+O)");
    AppendMenuW(file_menu, MF_STRING, 2, L"Оглавление (Ctrl+T)");
    AppendMenuW(file_menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(file_menu, MF_STRING, 3, L"Выход");
    AppendMenuW(menu, MF_POPUP, (UINT_PTR)file_menu, L"Файл");
    
    HWND hwnd = CreateWindowExW(
        0,
        L"EpubReaderClass",
        L"EPUB Reader",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT,
        600, 800,
        nullptr, menu, hinstance, nullptr
    );
    
    if (!hwnd) {
        LOG_ERROR("Failed to create window");
        return 1;
    }
    
    LOG_INFO("Main window created");
    
    ShowWindow(hwnd, cmdshow);
    UpdateWindow(hwnd);
    
    LOG_INFO("Entering message loop");
    
    MSG msg;
    while (GetMessage(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    
    LOG_INFO("=== EPUB Reader exiting ===");
    
    return (int)msg.wParam;
    
    } catch (const std::exception& e) {
        LOG_ERROR("Exception:", e.what());
        return 1;
    } catch (...) {
        LOG_ERROR("Unknown exception");
        return 1;
    }
}