
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shellapi.h>
#include <objbase.h>
#include <shobjidl.h>
#include <commctrl.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#define MAGIC_TABLE   "LEKSUS01"
#define MAGIC_TRAILER "LEKSUSZZ"

typedef unsigned long long u64;
typedef unsigned int       u32;
typedef unsigned short     u16;

static FILE    *g_exe;
static u64      g_base, g_payload_size;
static wchar_t  g_dir[MAX_PATH * 2], g_self[MAX_PATH * 4];
static HWND g_window, g_status_control, g_progress, g_button;
static HFONT g_font_title, g_font_body;
static HBRUSH g_background;
static int g_started, g_finished, g_install_result;
static char g_status_buffer[4096];
static void wide_from_utf8(const char *s, wchar_t *out, int cap);

static void pump_ui(void)
{
    MSG msg;
    while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    if (g_window) UpdateWindow(g_window);
}
static void показать_статус(const char *utf8)
{
    wchar_t wide[1024];
    if (!g_status_control || !utf8) return;
    wide_from_utf8(utf8, wide, 1024);
    SetWindowTextW(g_status_control, wide);
}
static void speak(const char *utf8)
{
    size_t used = strlen(g_status_buffer), add = strlen(utf8);
    if (add > sizeof(g_status_buffer) - used - 1) add = sizeof(g_status_buffer) - used - 1;
    if (add) { memcpy(g_status_buffer + used, utf8, add); g_status_buffer[used + add] = 0; }
    const char *line = strrchr(g_status_buffer, '\n');
    line = line ? line + 1 : g_status_buffer;
    if (*line) показать_статус(line);
    pump_ui();
}

static void wide_from_utf8(const char *s, wchar_t *out, int cap)
{
    int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, out, cap);
    if (n <= 0) out[0] = 0;
}

static void error_box(const char *utf8)
{
    wchar_t w[2048];
    wide_from_utf8(utf8, w, 2048);
    MessageBoxW(NULL, w, L"Лексус", MB_ICONERROR | MB_OK);
    speak(utf8);
    speak("\n");
}

static int read_exact(void *buf, size_t size)
{
    return fread(buf, 1, size, g_exe) == size;
}

static int seek_to(u64 pos)
{
    return _fseeki64(g_exe, (long long)pos, SEEK_SET) == 0;
}

static void make_parent_dirs(const wchar_t *path)
{
    wchar_t buf[MAX_PATH * 4];
    size_t i, len = wcslen(path);

    if (len >= sizeof(buf) / sizeof(wchar_t)) return;
    wcscpy(buf, path);

    for (i = 0; i < len; i++) {
        if (buf[i] == L'\\' || buf[i] == L'/') {
            wchar_t save = buf[i];
            buf[i] = 0;
            if (wcslen(buf) > 2) CreateDirectoryW(buf, NULL);
            buf[i] = save;
        }
    }
}

static void сказать_код(const char *что, DWORD код)
{
    char buf[128];
    char num[24];
    sprintf(num, "%lu", (unsigned long)код);
    sprintf(buf, "%s (код %s)\n", что, num);
    speak(buf);
}

static int copy_file(const wchar_t *target, u64 offset, u64 size)
{
    wchar_t tmp[64] = {0};
    char  chunk[65536];
    u64   left = size;
    FILE *out;

    make_parent_dirs(target);
    out = _wfopen(target, L"wb");
    if (!out) {
        сказать_код("  файл не открылся", GetLastError());
        return 0;
    }
    if (!seek_to(offset)) { fclose(out); return 0; }

    while (left > 0) {
        size_t want = left > sizeof(chunk) ? sizeof(chunk) : (size_t)left;
        size_t got = fread(chunk, 1, want, g_exe);
        if (got == 0) { fclose(out); return 0; }
        if (fwrite(chunk, 1, got, out) != got) { fclose(out); return 0; }
        left -= got;
    }
    fclose(out);
    (void)tmp;
    return 1;
}

static int file_exists(const wchar_t *path)
{
    DWORD a = GetFileAttributesW(path);
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

static int load_payload(u64 *offset, u32 *count)
{
    long long total;
    char magic[8];
    u64 size;

    if (_fseeki64(g_exe, 0, SEEK_END) != 0) return 0;
    total = _ftelli64(g_exe);
    if (total < 24) return 0;
    if (!seek_to((u64)(total - 24))) return 0;
    if (!read_exact(magic, 8) || memcmp(magic, MAGIC_TRAILER, 8) != 0) return 0;
    if (!read_exact(offset, 8) || !read_exact(&size, 8)) return 0;
    if (*offset == 0 || size < 12 || *offset > (u64)total - 24 || size > (u64)total - 24 - *offset) return 0;
    g_base = *offset;
    g_payload_size = size;
    if (!seek_to(*offset)) return 0;
    if (!read_exact(magic, 8) || memcmp(magic, MAGIC_TABLE, 8) != 0) return 0;
    if (!read_exact(count, 4)) return 0;
    return 1;
}

typedef struct {
    char  имя[MAX_PATH * 2];
    u64   размер;
    u64   смещение;
} запись;

static запись *g_записи;

static int безопасный_путь(const char *имя)
{
    const char *p = имя;
    if (!имя || !*имя || имя[0] == '/' || имя[0] == '\\') return 0;
    if (strchr(имя, ':') || strchr(имя, '\\')) return 0;
    while (*p) {
        const char *start = p;
        while (*p && *p != '/') ++p;
        size_t n = (size_t)(p - start);
        if (!n || (n == 1 && start[0] == '.') ||
            (n == 2 && start[0] == '.' && start[1] == '.')) return 0;
        if (*p == '/') ++p;
    }
    return 1;
}
static int read_table(u32 count)
{
    u32 i;
    u64 table_end;
    if (!count || count > 100000) return 0;
    g_записи = (запись *)calloc(count, sizeof(запись));
    if (!g_записи) return 0;

    for (i = 0; i < count; i++) {
        u16 len = 0;
        if (!read_exact(&len, 2)) return 0;
        if (len == 0 || len >= sizeof(g_записи[i].имя)) return 0;
        if (!read_exact(g_записи[i].имя, len)) return 0;
        g_записи[i].имя[len] = 0;
        if (!безопасный_путь(g_записи[i].имя)) return 0;
        {
            wchar_t проверка[MAX_PATH * 2];
            if (!MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, g_записи[i].имя,
                                     -1, проверка, MAX_PATH * 2)) return 0;
        }
        if (!read_exact(&g_записи[i].размер, 8) || !read_exact(&g_записи[i].смещение, 8)) return 0;
        if (g_записи[i].смещение > g_payload_size ||
            g_записи[i].размер > g_payload_size - g_записи[i].смещение) return 0;
    }
    {
        long long pos = _ftelli64(g_exe);
        if (pos < 0 || (u64)(pos - (long long)g_base) > g_payload_size) return 0;
        table_end = (u64)(pos - (long long)g_base);
    }
    for (i = 0; i < count; ++i)
        if (g_записи[i].смещение < table_end) return 0;
    return 1;
}

static int extract_all(u32 count)
{
    u32 i;
    int errors = 0;

    for (i = 0; i < count; i++) {
        wchar_t whname[MAX_PATH * 2];
        wchar_t target[MAX_PATH * 4];
        size_t  k;

        wide_from_utf8(g_записи[i].имя, whname, MAX_PATH * 2);
        for (k = 0; whname[k]; k++)
            if (whname[k] == L'/') whname[k] = L'\\';

        wcscpy(target, g_dir);
        wcscat(target, L"\\");
        wcscat(target, whname);

        if (!copy_file(target, g_base + g_записи[i].смещение, g_записи[i].размер)) {
            errors++;
            speak("Ошибка записи файла: ");
            speak(g_записи[i].имя);
            speak("\n");
        }
        if (g_progress) {
            SendMessageW(g_progress, PBM_SETPOS, (WPARAM)(i + 1), 0);
            char status[160];
            sprintf(status, "Распаковка: %u из %u файлов", (unsigned)(i + 1), (unsigned)count);
            показать_статус(status);
            pump_ui();
        }
    }
    return errors;
}

static int make_shortcut(const wchar_t *lnk, const wchar_t *target,
                         const wchar_t *args, const wchar_t *work,
                         const wchar_t *icon)
{
    IShellLinkW *sl = NULL;
    IPersistFile *pf = NULL;
    int ok = 0;

    if (SUCCEEDED(CoCreateInstance(&CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                   &IID_IShellLinkW, (void **)&sl))) {
        sl->lpVtbl->SetPath(sl, target);
        sl->lpVtbl->SetArguments(sl, args);
        sl->lpVtbl->SetWorkingDirectory(sl, work);
        sl->lpVtbl->SetDescription(sl, L"Язык программирования Лексика");
        if (icon && icon[0]) sl->lpVtbl->SetIconLocation(sl, icon, 0);
        if (SUCCEEDED(sl->lpVtbl->QueryInterface(sl, &IID_IPersistFile, (void **)&pf))) {
            if (SUCCEEDED(pf->lpVtbl->Save(pf, lnk, TRUE))) ok = 1;
            pf->lpVtbl->Release(pf);
        }
        sl->lpVtbl->Release(sl);
    }
    return ok;
}

static void write_cmd(const wchar_t *path, const wchar_t *body)
{
    FILE *f = _wfopen(path, L"wb");
    char *u8;
    int   n;
    if (!f) return;

    n = WideCharToMultiByte(CP_UTF8, 0, body, -1, NULL, 0, NULL, NULL);
    if (n > 1) {
        u8 = (char *)malloc((size_t)n);
        if (u8) {
            WideCharToMultiByte(CP_UTF8, 0, body, -1, u8, n, NULL, NULL);
            fprintf(f, "@chcp 65001 >nul\r\n@echo off\r\nsetlocal\r\n");
            fwrite(u8, 1, (size_t)(n - 1), f);
            free(u8);
        }
    }
    fclose(f);
}

static int run_and_wait(const wchar_t *exe, const wchar_t *args)
{
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    wchar_t line[8192];
    DWORD code = 1;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    wcscpy(line, L"\"");
    wcscat(line, exe);
    wcscat(line, L"\" ");
    wcscat(line, args);

    if (CreateProcessW(NULL, line, NULL, NULL, TRUE, CREATE_NO_WINDOW, NULL, g_dir, &si, &pi)) {
        DWORD ожидание, начало = GetTickCount();
        do {
            ожидание = MsgWaitForMultipleObjects(1, &pi.hProcess, FALSE, 200, QS_ALLINPUT);
            if (ожидание == WAIT_OBJECT_0 + 1) pump_ui();
            if (GetTickCount() - начало >= 120000) {
                TerminateProcess(pi.hProcess, 1460);
                WaitForSingleObject(pi.hProcess, 5000);
                code = 1460;
                break;
            }
        } while (ожидание == WAIT_TIMEOUT || ожидание == WAIT_OBJECT_0 + 1);
        if (ожидание == WAIT_OBJECT_0) GetExitCodeProcess(pi.hProcess, &code);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    return (int)code;
}

static int g_глубина = 0;

static void remove_tree(const wchar_t *dir)
{

    WIN32_FIND_DATAW fd;
    wchar_t mask[MAX_PATH * 4];
    HANDLE h;

    wcscpy(mask, dir);
    wcscat(mask, L"\\*");
    h = FindFirstFileW(mask, &fd);
    if (h != INVALID_HANDLE_VALUE) {
        do {
            wchar_t child[MAX_PATH * 4];
            if (!wcscmp(fd.cFileName, L".") || !wcscmp(fd.cFileName, L".."))
                continue;
            wcscpy(child, dir);
            wcscat(child, L"\\");
            wcscat(child, fd.cFileName);
            if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
                remove_tree(child);
            } else {
                SetFileAttributesW(child, FILE_ATTRIBUTE_NORMAL);
                DeleteFileW(child);
            }
        } while (FindNextFileW(h, &fd));
        FindClose(h);
    }
    RemoveDirectoryW(dir);
    speak("удалено: "); speak("каталог установки\n");
}

static void удалить_ярлыки(void)
{
    wchar_t папка[MAX_PATH], путь[MAX_PATH * 2];
    if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, папка) == S_OK) {
        wcscpy(путь, папка); wcscat(путь, L"\\Лексика.lnk"); DeleteFileW(путь);
    }
    if (SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, папка) == S_OK) {
        wcscpy(путь, папка); wcscat(путь, L"\\Лексика.lnk"); DeleteFileW(путь);
    }
}

static int выполнить_установку(int argc, char **argv)
{
    int wargc = 0;
    wchar_t **wargv = CommandLineToArgvW(GetCommandLineW(), &wargc);
    u64 offset = 0;
    u32 count = 0;
    (void)argc;
    (void)argv;
    int errors;
    wchar_t python[MAX_PATH * 4];
    wchar_t нативное[MAX_PATH * 4];
    wchar_t entry[MAX_PATH * 4];
    wchar_t cmd[MAX_PATH * 4];
    wchar_t значок[MAX_PATH * 4];
    wchar_t desktop[MAX_PATH];
    wchar_t startmenu[MAX_PATH];
    wchar_t lnk[MAX_PATH * 2];

    SetEnvironmentVariableW(L"PYTHONUTF8", L"1");
    SetEnvironmentVariableW(L"PYTHONIOENCODING", L"utf-8");

    speak("\n  Лексус 1.0 — установщик языка Лексика\n");
    speak("  язык программирования, где всё по-русски\n\n");

    if (!GetModuleFileNameW(NULL, g_self, MAX_PATH * 4)) {
        error_box("не удалось определить путь к установщику");
        return 1;
    }
    g_exe = _wfopen(g_self, L"rb");
    if (!g_exe) {
        error_box("не удалось открыть файл установщика");
        return 1;
    }

    if (GetEnvironmentVariableW(L"LOCALAPPDATA", g_dir, MAX_PATH) == 0) {
        if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, g_dir) != S_OK) {
            wcscpy(g_dir, L"C:");
        }
    }
    {
        size_t дл = wcslen(g_dir);
        while (дл > 0 && (g_dir[дл - 1] == L'\\' || g_dir[дл - 1] == L'/'))
            g_dir[--дл] = 0;
    }
    wcscat(g_dir, L"\\Лексус");

    if (wargc > 1 && (!wcscmp(wargv[1], L"--удалить") || !wcscmp(wargv[1], L"/удалить")
                      || !wcscmp(wargv[1], L"--uninstall"))) {
        CoInitialize(NULL);
        remove_tree(g_dir);
        удалить_ярлыки();
        speak("\nЛексус снят. Пользовательские настройки сохранены.\n");
        CoUninitialize();
        return 0;
    }

    speak("  ставлю в ");
    {
        char utf8[MAX_PATH * 4];
        WideCharToMultiByte(CP_UTF8, 0, g_dir, -1, utf8, sizeof(utf8), NULL, NULL);
        speak(utf8);
    }
    speak("\n\n");

    if (!load_payload(&offset, &count)) {
        error_box("груз не найден: файл установщика повреждён или это не установщик Лексуса");
        return 2;
    }
    speak("  груз найден, записей: ");
    {
        char num[32];
        sprintf(num, "%u", count);
        speak(num);
    }
    speak("\n");

    if (!CreateDirectoryW(g_dir, NULL) && GetLastError() != ERROR_ALREADY_EXISTS) {
        сказать_код("каталог установки не создан", GetLastError());
        fclose(g_exe); g_exe = NULL; return 3;
    }
    speak("Каталог установки готов.\n");
    if (!read_table(count)) {
        error_box("Пакет повреждён: оглавление, путь или смещение файла некорректны.");
        fclose(g_exe); g_exe = NULL; return 3;
    }
    if (g_progress) SendMessageW(g_progress, PBM_SETRANGE32, 0, (LPARAM)count);
    errors = extract_all(count);
    if (errors) {
        error_box("Не все файлы удалось распаковать. Проверьте свободное место и права на папку установки.");
        fclose(g_exe); g_exe = NULL; return 4;
    }
    speak("Файлы распакованы: ");
    {
        char num[32];
        sprintf(num, "%u", count - (unsigned)errors);
        speak(num);
    }
    speak("\n  ошибок записи: ");
    {
        char num[32];
        sprintf(num, "%d", errors);
        speak(num);
    }
    speak("\n");

    wcscpy(python, g_dir);
    wcscat(python, L"\\питон\\python.exe");
    wcscpy(entry, g_dir);
    wcscat(entry, L"\\лексус.lek");
    wcscpy(нативное, g_dir);
    wcscat(нативное, L"\\лексус-ядро.exe");

    wcscpy(cmd, g_dir);
    wcscat(cmd, L"\\лексус.cmd");

    /* Используем ресурс иконки только если он действительно встроен в EXE. */
    wcscpy(значок, нативное);
    {
        HICON большая = NULL, малая = NULL;
        UINT есть_значок = ExtractIconExW(нативное, 0, &большая, &малая, 1);
        if (большая) DestroyIcon(большая);
        if (малая) DestroyIcon(малая);
        if (есть_значок == 0) wcscpy(значок, g_self);
    }
    {
        wchar_t body[4096];
        wcscpy(body, L"set PYTHONUTF8=1\r\n");
        wcscat(body, L"set PYTHONIOENCODING=utf-8\r\n");
        wcscat(body, L"if not exist \"%~dp0питон\\python.exe\" (echo Встроенная среда не найдена.& exit /b 2)\r\n");
        wcscat(body, L"\"%~dp0питон\\python.exe\" \"%~dp0лексус.lek\" %*\r\n");
        write_cmd(cmd, body);
    }

    {
        if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop) == S_OK) {
            CoInitialize(NULL);
            wcscpy(lnk, desktop);
            wcscat(lnk, L"\\Лексика.lnk");

            if (make_shortcut(lnk, cmd, L"", g_dir, значок)) speak("  ярлык на рабочем столе\n");
            CoUninitialize();
        }
        if (SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, startmenu) == S_OK) {
            wcscpy(lnk, startmenu);
            wcscat(lnk, L"\\Лексика.lnk");
            CoInitialize(NULL);
            if (make_shortcut(lnk, cmd, L"", g_dir, значок)) speak("  ярлык в меню «Пуск»\n");
            CoUninitialize();
        }
    }

    /* PATH и пользовательские настройки не меняются. */

    speak("\n  самопроверка:\n");
    if (file_exists(нативное)) {
        if (run_and_wait(нативное, L"--self-test") == 0)
            speak("  нативное ядро: проверка пройдена\n");
        else
            speak("  нативное ядро: проверка не прошла\n");
    }
    if (!file_exists(python)) {
        error_box("Встроенная среда выполнения отсутствует. Установка остановлена.");
        fclose(g_exe); g_exe = NULL; return 5;
    }
    {
        wchar_t wcheck[4096];
        wcscpy(wcheck, L"\"");
        wcscat(wcheck, entry);
        wcscat(wcheck, L"\" проверить \"");
        wcscat(wcheck, g_dir);
        wcscat(wcheck, L"\\примеры\\привет.lek\"");
        if (run_and_wait(python, wcheck) != 0) {
            error_box("Самопроверка Лексики не прошла. Установка не считается завершённой.");
            fclose(g_exe); g_exe = NULL; return 6;
        }
        speak("Самопроверка пройдена.\n");
    }

    speak("\nУстановка завершена. Лексика готова к локальной работе.\n");
    speak("Запуск: лексус.cmd запустить примеры\\привет.lek\n");
    fclose(g_exe); g_exe = NULL;
    return 0;
}


static LRESULT CALLBACK окно_процедура(HWND окно, UINT сообщение, WPARAM w, LPARAM l)
{
    switch (сообщение) {
    case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)w, RGB(226, 232, 242));
        SetBkMode((HDC)w, TRANSPARENT);
        return (LRESULT)g_background;
    case WM_COMMAND:
        if (LOWORD(w) == 1001) {
            if (!g_started) {
                g_started = 1;
                EnableWindow(g_button, FALSE);
                SetWindowTextW(g_button, L"Установка...");
                показать_статус("Подготовка локального пакета...");
                g_install_result = выполнить_установку(0, NULL);
                g_finished = 1;
                if (!g_install_result) {
                    показать_статус("Готово. Лексика установлена на этом компьютере.");
                    SetWindowTextW(g_button, L"Открыть папку");
                } else {
                    показать_статус("Установка остановлена. Закройте окно и проверьте сообщение.");
                    SetWindowTextW(g_button, L"Закрыть");
                }
                EnableWindow(g_button, TRUE);
            } else if (g_finished && !g_install_result) {
                ShellExecuteW(окно, L"open", g_dir, NULL, NULL, SW_SHOWNORMAL);
            } else DestroyWindow(окно);
            return 0;
        }
        break;
    case WM_CLOSE: DestroyWindow(окно); return 0;
    case WM_DESTROY: PostQuitMessage(0); return 0;
    }
    return DefWindowProcW(окно, сообщение, w, l);
}

int WINAPI WinMain(HINSTANCE instance, HINSTANCE previous, LPSTR cmdline, int show)
{
    (void)previous; (void)cmdline;
    const wchar_t *аргументы = GetCommandLineW();
    if (wcsstr(аргументы, L"--тихо") || wcsstr(аргументы, L"/S") ||
        wcsstr(аргументы, L"--удалить") || wcsstr(аргументы, L"--uninstall"))
        return выполнить_установку(0, NULL);
    INITCOMMONCONTROLSEX controls;
    controls.dwSize = sizeof(controls);
    controls.dwICC = ICC_PROGRESS_CLASS;
    InitCommonControlsEx(&controls);
    g_background = CreateSolidBrush(RGB(12, 17, 29));
    WNDCLASSW wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.lpfnWndProc = окно_процедура;
    wc.hInstance = instance;
    wc.lpszClassName = L"LeksusInstallerWindow";
    wc.hCursor = LoadCursorW(NULL, IDC_ARROW);
    wc.hIcon = LoadIconW(instance, MAKEINTRESOURCEW(1));
    wc.hbrBackground = g_background;
    RegisterClassW(&wc);
    g_window = CreateWindowExW(0, wc.lpszClassName, L"Лексика — установка",
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX,
        CW_USEDEFAULT, CW_USEDEFAULT, 720, 440, NULL, NULL, instance, NULL);
    if (!g_window) return 1;
    g_font_title = CreateFontW(34,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
    g_font_body = CreateFontW(17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
    HWND title = CreateWindowW(L"STATIC",L"ЛЕКСИКА",WS_CHILD|WS_VISIBLE,42,36,600,52,g_window,NULL,instance,NULL);
    HWND subtitle = CreateWindowW(L"STATIC",L"Локальная среда программирования",WS_CHILD|WS_VISIBLE,44,94,630,34,g_window,NULL,instance,NULL);
    HWND details = CreateWindowW(L"STATIC",
        L"Пакет уже встроен в установщик — интернет во время установки не нужен.\r\nФайлы будут размещены в профиле текущего пользователя.",
        WS_CHILD|WS_VISIBLE,44,158,630,58,g_window,NULL,instance,NULL);
    g_status_control = CreateWindowW(L"STATIC",L"Готов к установке",WS_CHILD|WS_VISIBLE,44,260,620,32,g_window,NULL,instance,NULL);
    g_progress = CreateWindowExW(0,PROGRESS_CLASSW,NULL,WS_CHILD|WS_VISIBLE|PBS_SMOOTH,
        44,302,620,18,g_window,NULL,instance,NULL);
    g_button = CreateWindowW(L"BUTTON",L"Установить локально",
        WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,440,346,224,42,g_window,
        (HMENU)(INT_PTR)1001,instance,NULL);
    SendMessageW(title,WM_SETFONT,(WPARAM)g_font_title,TRUE);
    SendMessageW(subtitle,WM_SETFONT,(WPARAM)g_font_body,TRUE);
    SendMessageW(details,WM_SETFONT,(WPARAM)g_font_body,TRUE);
    SendMessageW(g_status_control,WM_SETFONT,(WPARAM)g_font_body,TRUE);
    SendMessageW(g_button,WM_SETFONT,(WPARAM)g_font_body,TRUE);
    ShowWindow(g_window,show); UpdateWindow(g_window);
    MSG msg;
    while (GetMessageW(&msg,NULL,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (g_font_title) DeleteObject(g_font_title);
    if (g_font_body) DeleteObject(g_font_body);
    if (g_background) DeleteObject(g_background);
    return 0;
}
