
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
static HWND g_window, g_status_control, g_progress, g_button, g_estimate, g_path, g_browse;
static HWND g_cert_option, g_giga_option, g_yandex_option;
static u64 g_bytes_total, g_bytes_done;
static ULONGLONG g_copy_started, g_last_update;
static HFONT g_font_title, g_font_body;
static HBRUSH g_background;
static int g_started, g_finished, g_install_result, g_phase;
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

/* Распаковка занимает 90% шкалы; оставшиеся 10% — самопроверка и настройка. */
static void обновить_ход(int принудительно)
{
    ULONGLONG now = GetTickCount64(), elapsed = now - g_copy_started;
    wchar_t текст[160];
    unsigned percent;
    if (!g_progress || (!принудительно && now - g_last_update < 100)) return;
    g_last_update = now;
    percent = g_bytes_total ? (unsigned)(90.0 * (double)g_bytes_done / (double)g_bytes_total) : 90;
    if (percent > 90) percent = 90;
    SendMessageW(g_progress, PBM_SETPOS, percent, 0);
    if (g_bytes_done && elapsed >= 1000 && g_bytes_done < g_bytes_total) {
        double seconds = ((double)elapsed / 1000.0) *
            ((double)(g_bytes_total - g_bytes_done) / (double)g_bytes_done);
        if (seconds > 86400) seconds = 86400;
        swprintf(текст, 160, L"%u%%  ·  распаковка: осталось примерно %.0f с", percent, seconds);
    } else {
        swprintf(текст, 160, L"%u%%  ·  оценка времени появится во время распаковки", percent);
    }
    SetWindowTextW(g_estimate, текст);
    pump_ui();
}

#define INSTALL_KEY L"Software\\Leksika"

static int проверить_каталог(const wchar_t *путь, wchar_t *выход)
{
    wchar_t полный[MAX_PATH * 2];
    DWORD n;
    if (!путь || wcslen(путь) < 3 || wcschr(путь, L';')) return 0;
    /* Только абсолютные пути на дисках, не корень диска и не слишком длинные пути. */
    if (!((путь[0] >= L'A' && путь[0] <= L'Z') || (путь[0] >= L'a' && путь[0] <= L'z')) ||
        путь[1] != L':' || (путь[2] != L'\\' && путь[2] != L'/')) return 0;
    n = GetFullPathNameW(путь, MAX_PATH * 2, полный, NULL);
    if (!n || n >= 220 || n <= 3) return 0;
    while (n > 3 && (полный[n - 1] == L'\\' || полный[n - 1] == L'/')) полный[--n] = 0;
    if (n <= 3) return 0;
    wcscpy(выход, полный);
    return 1;
}

static int сохранённый_каталог(wchar_t *out)
{
    HKEY ключ; DWORD тип = 0, размер = sizeof(wchar_t) * MAX_PATH * 2;
    wchar_t путь[MAX_PATH * 2] = {0};
    if (RegOpenKeyExW(HKEY_CURRENT_USER, INSTALL_KEY, 0, KEY_QUERY_VALUE, &ключ) != ERROR_SUCCESS) return 0;
    LONG код = RegQueryValueExW(ключ, L"InstallDir", NULL, &тип, (BYTE *)путь, &размер);
    RegCloseKey(ключ);
    путь[MAX_PATH * 2 - 1] = 0;
    return код == ERROR_SUCCESS && тип == REG_SZ && проверить_каталог(путь, out);
}

static int сохранить_каталог(void)
{
    HKEY ключ;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, INSTALL_KEY, 0, NULL, 0, KEY_SET_VALUE, NULL, &ключ, NULL) != ERROR_SUCCESS) return 0;
    LONG код = RegSetValueExW(ключ, L"InstallDir", 0, REG_SZ, (BYTE *)g_dir,
                              (DWORD)((wcslen(g_dir) + 1) * sizeof(wchar_t)));
    RegCloseKey(ключ);
    return код == ERROR_SUCCESS;
}

static int каталог_установки(void)
{
    wchar_t путь[MAX_PATH * 2];
    DWORD n;
    if (сохранённый_каталог(g_dir)) return 1;
    if (GetDriveTypeW(L"D:\\") != DRIVE_NO_ROOT_DIR && GetDriveTypeW(L"D:\\") != DRIVE_UNKNOWN)
        return проверить_каталог(L"D:\\Лексика", g_dir);
    n = GetEnvironmentVariableW(L"LOCALAPPDATA", путь, MAX_PATH);
    if (!n || n >= MAX_PATH) {
        if (SHGetFolderPathW(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, путь) != S_OK) return 0;
    }
    if (wcslen(путь) + 9 >= MAX_PATH * 2) return 0;
    wcscat(путь, L"\\Лексика");
    return проверить_каталог(путь, g_dir);
}

/* Не работаем через точки перехода, чтобы не затрагивать чужие деревья. */
static int нет_переходов(const wchar_t *путь)
{
    wchar_t часть[MAX_PATH * 4]; size_t n = wcslen(путь);
    if (n >= MAX_PATH * 4) return 0;
    wcscpy(часть, путь);
    for (size_t i = 3; i <= n; ++i) {
        if (часть[i] == L'\\' || часть[i] == L'/' || часть[i] == 0) {
            wchar_t старый = часть[i]; часть[i] = 0;
            DWORD атрибуты = GetFileAttributesW(часть);
            часть[i] = старый;
            if (атрибуты != INVALID_FILE_ATTRIBUTES && (атрибуты & FILE_ATTRIBUTE_REPARSE_POINT)) return 0;
        }
    }
    return 1;
}

static int пустой_каталог(const wchar_t *путь)
{
    wchar_t маска[MAX_PATH * 2 + 4]; WIN32_FIND_DATAW данные; HANDLE поиск;
    swprintf(маска, MAX_PATH * 2 + 4, L"%ls\\*", путь);
    поиск = FindFirstFileW(маска, &данные);
    if (поиск == INVALID_HANDLE_VALUE) return GetFileAttributesW(путь) == INVALID_FILE_ATTRIBUTES;
    do {
        if (wcscmp(данные.cFileName, L".") && wcscmp(данные.cFileName, L"..")) {
            FindClose(поиск); return 0;
        }
    } while (FindNextFileW(поиск, &данные));
    FindClose(поиск); return 1;
}

static void выбрать_каталог(HWND окно)
{
    BROWSEINFOW info; PIDLIST_ABSOLUTE выбран; wchar_t путь[MAX_PATH * 2], полный[MAX_PATH * 2];
    ZeroMemory(&info, sizeof(info));
    info.hwndOwner = окно;
    info.lpszTitle = L"Выберите папку для установки Лексики";
    info.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    CoInitialize(NULL);
    выбран = SHBrowseForFolderW(&info);
    if (выбран) {
        if (SHGetPathFromIDListW(выбран, путь)) {
            size_t len = wcslen(путь);
            if (len == 3 && путь[1] == L':') wcscat(путь, L"Лексика");
            if (проверить_каталог(путь, полный)) {
                wcscpy(g_dir, полный);
                SetWindowTextW(g_path, g_dir);
            } else MessageBoxW(окно, L"Выберите папку на диске с путём короче 220 символов.", L"Папка установки", MB_OK | MB_ICONWARNING);
        }
        CoTaskMemFree(выбран);
    }
    CoUninitialize();
}

static int copy_file(const wchar_t *target, u64 offset, u64 size)
{
    wchar_t tmp[64] = {0};
    char  chunk[65536];
    u64   left = size;
    FILE *out;

    if (!нет_переходов(target)) return 0;
    make_parent_dirs(target);
    if (!нет_переходов(target)) return 0;
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
        g_bytes_done += got;
        обновить_ход(0);
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
        if (errors) break;
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

/* Удаляем только файлы установочного пакета и пустые родительские папки. */
static void удалить_файл_пакета(const wchar_t *относительный)
{
    wchar_t путь[MAX_PATH * 4];
    if (swprintf(путь, MAX_PATH * 4, L"%ls\\%ls", g_dir, относительный) < 0 || !нет_переходов(путь)) return;
    DeleteFileW(путь);
    while (1) {
        wchar_t *разделитель = wcsrchr(путь, L'\\');
        if (!разделитель || (size_t)(разделитель - путь) <= wcslen(g_dir)) break;
        *разделитель = 0;
        RemoveDirectoryW(путь);
    }
}

static void удалить_файлы(u32 count)
{
    const wchar_t *доп[] = {L"Leksika.cmd", L"лексус.cmd", L"лексика.cmd", L"лек.cmd", L"лекс.cmd", L"лексу.cmd", L"leksus.cmd", L"lek.cmd", L"leks.cmd", L"Leksika-терминал.cmd"};
    for (u32 i = 0; i < count; ++i) {
        wchar_t имя[MAX_PATH * 2];
        wide_from_utf8(g_записи[i].имя, имя, MAX_PATH * 2);
        for (size_t k = 0; имя[k]; ++k) if (имя[k] == L'/') имя[k] = L'\\';
        удалить_файл_пакета(имя);
    }
    for (size_t i = 0; i < sizeof(доп)/sizeof(доп[0]); ++i) удалить_файл_пакета(доп[i]);
    RemoveDirectoryW(g_dir);
}

static int элемент_PATH_равен(const wchar_t *начало, size_t длина,
                              const wchar_t *цель)
{
    size_t длина_цели = wcslen(цель);
    while (длина && (*начало == L' ' || *начало == L'\t' || *начало == L'"')) {
        ++начало;
        --длина;
    }
    while (длина && (начало[длина - 1] == L' ' || начало[длина - 1] == L'\t' ||
                     начало[длина - 1] == L'"')) --длина;
    while (длина && (начало[длина - 1] == L'\\' || начало[длина - 1] == L'/')) --длина;
    while (длина_цели && (цель[длина_цели - 1] == L'\\' ||
                          цель[длина_цели - 1] == L'/')) --длина_цели;
    return длина == длина_цели &&
           CompareStringOrdinal(начало, (int)длина, цель, (int)длина_цели, TRUE) == CSTR_EQUAL;
}

/* Добавляет каталог Лексики в пользовательский PATH или удаляет его при деинсталляции. */
static int изменить_PATH_пользователя(int добавить)
{
    HKEY ключ = NULL;
    DWORD тип = REG_EXPAND_SZ, байты = 0;
    wchar_t *старый = NULL, *новый = NULL;
    size_t длина, длина_папки = wcslen(g_dir), ёмкость;
    const wchar_t *p;
    int найден = 0, записано = 0;
    LONG статус;

    статус = RegCreateKeyExW(HKEY_CURRENT_USER, L"Environment", 0, NULL, 0,
                             KEY_QUERY_VALUE | KEY_SET_VALUE, NULL, &ключ, NULL);
    if (статус != ERROR_SUCCESS) return 0;

    статус = RegQueryValueExW(ключ, L"Path", NULL, &тип, NULL, &байты);
    if (статус != ERROR_SUCCESS && статус != ERROR_FILE_NOT_FOUND) goto конец;
    if (статус == ERROR_SUCCESS && тип != REG_SZ && тип != REG_EXPAND_SZ) goto конец;

    старый = (wchar_t *)calloc((size_t)байты / sizeof(wchar_t) + 2, sizeof(wchar_t));
    if (!старый) goto конец;
    if (байты && RegQueryValueExW(ключ, L"Path", NULL, &тип,
                                  (BYTE *)старый, &байты) != ERROR_SUCCESS) goto конец;
    длина = wcslen(старый);

    p = старый;
    while (*p) {
        const wchar_t *конец = wcschr(p, L';');
        size_t часть = конец ? (size_t)(конец - p) : wcslen(p);
        if (элемент_PATH_равен(p, часть, g_dir)) найден = 1;
        if (!конец) break;
        p = конец + 1;
    }
    if ((добавить && найден) || (!добавить && !найден)) {
        записано = 1;
        goto конец;
    }

    ёмкость = длина + длина_папки + 2;
    новый = (wchar_t *)calloc(ёмкость, sizeof(wchar_t));
    if (!новый) goto конец;

    if (добавить) {
        wcscpy(новый, старый);
        if (длина && новый[длина - 1] != L';') wcscat(новый, L";");
        wcscat(новый, g_dir);
    } else {
        const wchar_t *часть_начала = старый;
        size_t использовано = 0;
        for (p = старый; ; ++p) {
            if (*p == L';' || *p == 0) {
                size_t часть = (size_t)(p - часть_начала);
                if (часть && !элемент_PATH_равен(часть_начала, часть, g_dir)) {
                    if (использовано) новый[использовано++] = L';';
                    memcpy(новый + использовано, часть_начала, часть * sizeof(wchar_t));
                    использовано += часть;
                }
                if (!*p) break;
                часть_начала = p + 1;
            }
        }
        новый[использовано] = 0;
    }

    {
        DWORD размер = (DWORD)((wcslen(новый) + 1) * sizeof(wchar_t));
        if (RegSetValueExW(ключ, L"Path", 0, тип, (const BYTE *)новый, размер) != ERROR_SUCCESS)
            goto конец;
    }
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, (LPARAM)L"Environment",
                        SMTO_ABORTIFHUNG, 5000, NULL);
    записано = 1;

конец:
    if (новый) free(новый);
    if (старый) free(старый);
    if (ключ) RegCloseKey(ключ);
    return записано;
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
    wchar_t legacy_cmd[MAX_PATH * 4];
    wchar_t terminal[MAX_PATH * 4];
    wchar_t shell[MAX_PATH];
    wchar_t значок[MAX_PATH * 4];
    wchar_t desktop[MAX_PATH];
    wchar_t startmenu[MAX_PATH];
    wchar_t lnk[MAX_PATH * 2];
    DWORD shell_length;

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

    if (!g_window && !каталог_установки()) {
        error_box("Не удалось определить каталог установки.");
        fclose(g_exe); g_exe = NULL; return 3;
    }
    if (!g_window && wargc > 2 && !wcscmp(wargv[1], L"--папка")) {
        wchar_t путь[MAX_PATH * 2];
        if (!проверить_каталог(wargv[2], путь)) {
            error_box("Некорректный путь установки."); fclose(g_exe); g_exe = NULL; return 3;
        }
        wcscpy(g_dir, путь);
    }
    if (wargc > 1 && (!wcscmp(wargv[1], L"--удалить") || !wcscmp(wargv[1], L"/удалить")
                      || !wcscmp(wargv[1], L"--uninstall"))) {
        if (!сохранённый_каталог(g_dir)) {
            wchar_t local[MAX_PATH * 2]; DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, MAX_PATH);
            if (!n || n >= MAX_PATH) { fclose(g_exe); g_exe = NULL; return 3; }
            wcscat(local, L"\\Лексус");
            if (!проверить_каталог(local, g_dir)) { fclose(g_exe); g_exe = NULL; return 3; }
            {
                wchar_t признак[MAX_PATH * 4];
                swprintf(признак, MAX_PATH * 4, L"%ls\\лексус-ядро.exe", g_dir);
                if (!file_exists(признак)) { fclose(g_exe); g_exe = NULL; return 3; }
                swprintf(признак, MAX_PATH * 4, L"%ls\\Leksika.cmd", g_dir);
                if (!file_exists(признак)) { fclose(g_exe); g_exe = NULL; return 3; }
            }
        }
        if (!нет_переходов(g_dir) || !load_payload(&offset, &count) || !read_table(count)) {
            error_box("Не удалось безопасно определить файлы для удаления."); fclose(g_exe); g_exe = NULL; return 3;
        }
        изменить_PATH_пользователя(0);
        удалить_файлы(count);
        удалить_ярлыки();
        RegDeleteKeyW(HKEY_CURRENT_USER, INSTALL_KEY);
        fclose(g_exe); g_exe = NULL;
        speak("Файлы Лексики удалены; остальные файлы сохранены.\n");
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

    {
        wchar_t прежний[MAX_PATH * 2];
        int своя = сохранённый_каталог(прежний) && !_wcsicmp(прежний, g_dir);
        if (!нет_переходов(g_dir) || (!своя && !пустой_каталог(g_dir))) {
            error_box("Выберите пустую папку: существующие файлы и точки перехода не перезаписываются.");
            fclose(g_exe); g_exe = NULL; return 3;
        }
        int код = SHCreateDirectoryExW(NULL, g_dir, NULL);
        if (код != ERROR_SUCCESS && код != ERROR_ALREADY_EXISTS && код != ERROR_FILE_EXISTS) {
            сказать_код("каталог установки не создан", (DWORD)код);
            fclose(g_exe); g_exe = NULL; return 3;
        }
    }
    speak("Каталог установки готов.\n");
    if (!read_table(count)) {
        error_box("Пакет повреждён: оглавление, путь или смещение файла некорректны.");
        fclose(g_exe); g_exe = NULL; return 3;
    }
    g_bytes_total = g_bytes_done = 0;
    for (u32 i = 0; i < count; ++i) {
        if (UINT64_MAX - g_bytes_total < g_записи[i].размер) {
            error_box("Пакет повреждён: неверный размер файлов.");
            fclose(g_exe); g_exe = NULL; return 3;
        }
        g_bytes_total += g_записи[i].размер;
    }
    g_copy_started = GetTickCount64(); g_last_update = 0;
    обновить_ход(1);
    errors = extract_all(count);
    if (errors) {
        error_box("Не все файлы удалось распаковать. Проверьте свободное место и права на папку установки.");
        fclose(g_exe); g_exe = NULL; return 4;
    }
    обновить_ход(1);
    if (g_estimate) SetWindowTextW(g_estimate, L"90%  ·  самопроверка и настройка");
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
    wcscat(cmd, L"\\Leksika.cmd");
    wcscpy(legacy_cmd, g_dir);
    wcscat(legacy_cmd, L"\\лексус.cmd");
    wcscpy(terminal, g_dir);
    wcscat(terminal, L"\\Leksika-терминал.cmd");

    shell_length = GetEnvironmentVariableW(L"COMSPEC", shell, MAX_PATH);
    if (shell_length == 0 || shell_length >= MAX_PATH)
        wcscpy(shell, L"C:\\Windows\\System32\\cmd.exe");

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
        wcscpy(body, L"if \"%~1\"==\"--нативно\" goto native\r\n");
        wcscat(body, L"set \"PYTHONUTF8=\"\r\n");
        wcscat(body, L"set \"PYTHONIOENCODING=utf-8\"\r\n");
        wcscat(body, L"if not exist \"%~dp0питон\\python.exe\" (echo Встроенная среда не найдена.& exit /b 2)\r\n");
        wcscat(body, L"\"%~dp0питон\\python.exe\" -X utf8 \"%~dp0лексус.lek\" %*\r\n");
        wcscat(body, L"exit /b %errorlevel%\r\n:native\r\n");
        wcscat(body, L"\"%~dp0лексус-ядро.exe\" %*\r\nexit /b %errorlevel%\r\n");
        write_cmd(cmd, body);
        {
            const wchar_t *имена[] = {L"лексус", L"лексика", L"лек", L"лекс", L"лексу", L"leksus", L"lek", L"leks"};
            size_t i;
            for (i = 0; i < sizeof(имена) / sizeof(имена[0]); ++i) {
                wchar_t путь[MAX_PATH * 4];
                if (wcslen(g_dir) + wcslen(имена[i]) + 6 >= sizeof(путь) / sizeof(путь[0])) {
                    error_box("Слишком длинный путь для команд Лексики.");
                    fclose(g_exe); g_exe = NULL; return 7;
                }
                wcscpy(путь, g_dir); wcscat(путь, L"\\");
                wcscat(путь, имена[i]); wcscat(путь, L".cmd");
                write_cmd(путь, L"\"%~dp0Leksika.cmd\" %*\r\n");
            }
        }
    }
    {
        wchar_t body[4096];
        wcscpy(body, L"cd /d \"%~dp0\"\r\n");
        wcscat(body, L"set \"PYTHONUTF8=\"\r\n");
        wcscat(body, L"set \"PYTHONIOENCODING=utf-8\"\r\n");
        wcscat(body, L"if not exist \"%~dp0питон\\python.exe\" (echo Встроенная среда не найдена.& exit /b 2)\r\n");
        wcscat(body, L"doskey Leksika=\"%~dp0Leksika.cmd\" $*\r\n");
        wcscat(body, L"doskey лексус=\"%~dp0Leksika.cmd\" $*\r\n");
        wcscat(body, L"\"%~dp0питон\\python.exe\" -X utf8 \"%~dp0лексус.lek\" справка\r\n");
        wcscat(body, L"echo.\r\n");
        wcscat(body, L"echo Вводите команды Лексики. Например: Leksika запустить примеры\\привет.lek\r\n");
        write_cmd(terminal, body);
    }

    {
        if (SHGetFolderPathW(NULL, CSIDL_DESKTOPDIRECTORY, NULL, 0, desktop) == S_OK) {
            CoInitialize(NULL);
            wcscpy(lnk, desktop);
            wcscat(lnk, L"\\Лексика.lnk");

            if (make_shortcut(lnk, shell, L"/k Leksika-терминал.cmd", g_dir, значок))
                speak("  ярлык на рабочем столе\n");
            CoUninitialize();
        }
        if (SHGetFolderPathW(NULL, CSIDL_PROGRAMS, NULL, 0, startmenu) == S_OK) {
            wcscpy(lnk, startmenu);
            wcscat(lnk, L"\\Лексика.lnk");
            CoInitialize(NULL);
            if (make_shortcut(lnk, shell, L"/k Leksika-терминал.cmd", g_dir, значок))
                speak("  ярлык в меню «Пуск»\n");
            CoUninitialize();
        }
    }

    /* PATH обновится только после успешной самопроверки. */

    speak("\n  самопроверка:\n");
    if (!file_exists(нативное)) {
        error_box("В установочном пакете отсутствует нативное ядро Лексуса.");
        fclose(g_exe); g_exe = NULL; return 7;
    }
    if (run_and_wait(нативное, L"--self-test") != 0) {
        error_box("Самопроверка нативного ядра не прошла. Установка не считается завершённой.");
        fclose(g_exe); g_exe = NULL; return 7;
    }
    speak("  нативное ядро: проверка пройдена\n");
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

    {
        wchar_t прежний[MAX_PATH * 2];
        int было = сохранённый_каталог(прежний);
        if (!сохранить_каталог()) {
            error_box("Не удалось сохранить путь установки. Путь в PATH не изменён.");
            fclose(g_exe); g_exe = NULL; return 8;
        }
        if (было && _wcsicmp(прежний, g_dir)) {
            wchar_t новый[MAX_PATH * 2]; wcscpy(новый, g_dir); wcscpy(g_dir, прежний);
            изменить_PATH_пользователя(0); wcscpy(g_dir, новый);
        }
    }
    if (изменить_PATH_пользователя(1))
        speak("Команда Leksika добавлена в пользовательский PATH. Откройте новое окно терминала.\n");
    else
        speak("Не удалось обновить PATH; команда доступна из каталога установки и окна Лексики.\n");

    speak("\nУстановка завершена. Лексика готова к работе.\n");
    speak("Запуск: Leksika запустить примеры\\привет.lek\n");
    fclose(g_exe); g_exe = NULL;
    return 0;
}


/* Загрузка идёт отдельно от основного установщика и не ограничена таймаутом самопроверки. */
static int запустить_загрузку_моделей(int giga, int yandex)
{
    wchar_t python[MAX_PATH * 4], скрипт[MAX_PATH * 4], команда[MAX_PATH * 12];
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    int n;
    n = swprintf(python, MAX_PATH * 4, L"%ls\\питон\\python.exe", g_dir);
    if (n < 0 || n >= MAX_PATH * 4) return 0;
    n = swprintf(скрипт, MAX_PATH * 4, L"%ls\\лексика\\скачать_модели.py", g_dir);
    if (n < 0 || n >= MAX_PATH * 4 || !file_exists(python) || !file_exists(скрипт)) return 0;
    n = swprintf(команда, MAX_PATH * 12, L"\"%ls\" -X utf8 \"%ls\" --окно %ls %ls",
                 python, скрипт, giga ? L"gigachat" : L"", yandex ? L"yandex" : L"");
    if (n < 0 || n >= MAX_PATH * 12) return 0;
    ZeroMemory(&si, sizeof(si)); si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    if (!CreateProcessW(python, команда, NULL, NULL, FALSE, CREATE_NEW_CONSOLE,
                        NULL, g_dir, &si, &pi)) return 0;
    CloseHandle(pi.hThread); CloseHandle(pi.hProcess);
    return 1;
}

static void нарисовать_фон(HWND окно)
{
    PAINTSTRUCT ps; HDC dc = BeginPaint(окно, &ps); RECT r;
    GetClientRect(окно, &r);
    FillRect(dc, &r, g_background);
    /* Тихое звёздное поле: без внешних изображений и тяжёлого декодирования. */
    for (int i = 0; i < 55; ++i) {
        int x = 10 + ((i * 137 + g_phase) % (r.right > 20 ? r.right - 20 : 1));
        int y = 10 + ((i * 83) % (r.bottom > 20 ? r.bottom - 20 : 1));
        int bright = 55 + ((i * 13 + g_phase) % 90);
        HBRUSH кисть = CreateSolidBrush(RGB(bright / 2, bright, bright + 40));
        HGDIOBJ старый = SelectObject(dc, кисть);
        HGDIOBJ перо = SelectObject(dc, GetStockObject(NULL_PEN));
        int размер = i % 4 == 0 ? 4 : 2;
        Ellipse(dc, x, y, x + размер, y + размер);
        SelectObject(dc, перо); SelectObject(dc, старый); DeleteObject(кисть);
    }
    EndPaint(окно, &ps);
}

static LRESULT CALLBACK окно_процедура(HWND окно, UINT сообщение, WPARAM w, LPARAM l)
{
    switch (сообщение) {
    case WM_PAINT: нарисовать_фон(окно); return 0;
    case WM_ERASEBKGND: return 1;
    case WM_TIMER:
        if (!IsIconic(окно)) {
            g_phase = (g_phase + 2) % 115;
            InvalidateRect(окно, NULL, FALSE);
        }
        return 0;
    case WM_CTLCOLORBTN:
        if ((HWND)l != g_cert_option && (HWND)l != g_giga_option && (HWND)l != g_yandex_option)
            break;
        SetTextColor((HDC)w, RGB(226, 232, 242));
        SetBkMode((HDC)w, TRANSPARENT);
        return (LRESULT)g_background;
    case WM_CTLCOLORSTATIC:
        SetTextColor((HDC)w, RGB(226, 232, 242));
        SetBkMode((HDC)w, TRANSPARENT);
        return (LRESULT)g_background;
    case WM_COMMAND:
        if ((LOWORD(w) == 1002 || LOWORD(w) == 1003) && !g_started) {
            выбрать_каталог(окно); return 0;
        }
        if (LOWORD(w) == 1001) {
            if (!g_started) {
                g_started = 1;
                EnableWindow(g_button, FALSE);
                EnableWindow(g_path, FALSE); EnableWindow(g_browse, FALSE);
                EnableWindow(g_cert_option, FALSE);
                EnableWindow(g_giga_option, FALSE);
                EnableWindow(g_yandex_option, FALSE);
                SetWindowTextW(g_button, L"Установка...");
                показать_статус("Подготовка пакета...");
                g_install_result = выполнить_установку(0, NULL);
                g_finished = 1;
                if (!g_install_result) {
                    показать_статус("Готово. Лексика установлена на этом компьютере.");
                    SendMessageW(g_progress, PBM_SETPOS, 100, 0);
                    SetWindowTextW(g_estimate, L"100%  ·  установка завершена");
                    SetWindowTextW(g_button, L"Открыть папку");
                    if (SendMessageW(g_cert_option, BM_GETCHECK, 0, 0) == BST_CHECKED) {
                        if (MessageBoxW(окно, L"Корневые сертификаты расширяют доверие Windows ко всем сертификатам, выданным этим центром. Откроется официальная страница Госуслуг; установка выполняется вами отдельно. Продолжить?", L"Сертификаты Минцифры", MB_ICONWARNING | MB_YESNO | MB_DEFBUTTON2) == IDYES)
                            ShellExecuteW(окно, L"open", L"https://www.gosuslugi.ru/crt", NULL, NULL, SW_SHOWNORMAL);
                    }
                    {
                        int giga = SendMessageW(g_giga_option, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        int yandex = SendMessageW(g_yandex_option, BM_GETCHECK, 0, 0) == BST_CHECKED;
                        if ((giga || yandex) && !запустить_загрузку_моделей(giga, yandex))
                            MessageBoxW(окно, L"Лексика установлена, но окно загрузки моделей не запустилось. Повторите загрузку вручную по инструкции в README.md.", L"Модели не загружены", MB_ICONWARNING | MB_OK);
                    }
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
    case WM_CLOSE:
        if (!g_started || g_finished) DestroyWindow(окно);
        return 0;
    case WM_DESTROY: KillTimer(окно, 1); PostQuitMessage(0); return 0;
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
        WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_CLIPCHILDREN,
        CW_USEDEFAULT, CW_USEDEFAULT, 760, 680, NULL, NULL, instance, NULL);
    if (!g_window) return 1;
    g_font_title = CreateFontW(34,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
    g_font_body = CreateFontW(17,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,
        OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH|FF_DONTCARE,L"Segoe UI");
    if (!каталог_установки()) { error_box("Не удалось определить каталог установки."); return 1; }
    HWND title = CreateWindowW(L"STATIC",L"ЛЕКСИКА",WS_CHILD|WS_VISIBLE,42,28,440,50,g_window,NULL,instance,NULL);
    HWND subtitle = CreateWindowW(L"STATIC",L"Среда программирования",WS_CHILD|WS_VISIBLE,44,82,440,28,g_window,NULL,instance,NULL);
    HWND path_label = CreateWindowW(L"STATIC",L"ПАПКА УСТАНОВКИ",WS_CHILD|WS_VISIBLE,44,132,660,24,g_window,NULL,instance,NULL);
    g_path = CreateWindowW(L"BUTTON",g_dir,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON|BS_LEFT,44,158,525,32,g_window,(HMENU)(INT_PTR)1002,instance,NULL);
    g_browse = CreateWindowW(L"BUTTON",L"Обзор...",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_PUSHBUTTON,579,158,120,32,g_window,(HMENU)(INT_PTR)1003,instance,NULL);
    HWND details = CreateWindowW(L"STATIC",L"Основной пакет встроен и устанавливается без интернета.",WS_CHILD|WS_VISIBLE,44,200,660,26,g_window,NULL,instance,NULL);
    HWND options = CreateWindowW(L"STATIC",L"ДОПОЛНИТЕЛЬНО  ·  выключено по умолчанию",WS_CHILD|WS_VISIBLE,44,246,660,28,g_window,NULL,instance,NULL);
    g_cert_option = CreateWindowW(L"BUTTON",L"Сертификаты Минцифры — страница Госуслуг",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,44,282,655,28,g_window,NULL,instance,NULL);
    g_giga_option = CreateWindowW(L"BUTTON",L"Скачать GigaChat 3.1 10B (≈ 6,5 ГБ)",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,44,318,655,28,g_window,NULL,instance,NULL);
    g_yandex_option = CreateWindowW(L"BUTTON",L"Скачать YandexGPT 5 Lite 8B (≈ 5 ГБ)",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,44,354,655,28,g_window,NULL,instance,NULL);
    HWND note = CreateWindowW(L"STATIC",L"Модели загружаются в отдельном окне; нужен интернет и llama.cpp.\r\nСертификаты: ручная установка на Госуслугах после предупреждения.",WS_CHILD|WS_VISIBLE,44,392,655,50,g_window,NULL,instance,NULL);
    g_status_control = CreateWindowW(L"STATIC",L"Готово к установке",WS_CHILD|WS_VISIBLE,44,454,655,26,g_window,NULL,instance,NULL);
    g_progress = CreateWindowExW(0,PROGRESS_CLASSW,NULL,WS_CHILD|WS_VISIBLE|PBS_SMOOTH,44,488,655,20,g_window,NULL,instance,NULL);
    SendMessageW(g_progress, PBM_SETRANGE32, 0, 100);
    SendMessageW(g_progress, PBM_SETBARCOLOR, 0, RGB(100, 180, 255));
    SendMessageW(g_progress, PBM_SETBKCOLOR, 0, RGB(37, 48, 68));
    g_estimate = CreateWindowW(L"STATIC",L"Время зависит от скорости диска",WS_CHILD|WS_VISIBLE,44,518,655,26,g_window,NULL,instance,NULL);
    g_button = CreateWindowW(L"BUTTON",L"Установить",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON,475,556,224,42,g_window,(HMENU)(INT_PTR)1001,instance,NULL);
    SendMessageW(title,WM_SETFONT,(WPARAM)g_font_title,TRUE);
    HWND items[] = {subtitle,path_label,g_path,g_browse,details,options,g_cert_option,g_giga_option,g_yandex_option,note,g_status_control,g_estimate,g_button};
    for (size_t i = 0; i < sizeof(items)/sizeof(items[0]); ++i)
        SendMessageW(items[i],WM_SETFONT,(WPARAM)g_font_body,TRUE);
    {
        BOOL анимация = TRUE;
        if (SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &анимация, 0) && анимация)
            SetTimer(g_window, 1, 500, NULL);
    }
    ShowWindow(g_window,show); UpdateWindow(g_window);
    MSG msg;
    while (GetMessageW(&msg,NULL,0,0)>0) { TranslateMessage(&msg); DispatchMessageW(&msg); }
    if (g_font_title) DeleteObject(g_font_title);
    if (g_font_body) DeleteObject(g_font_body);
    if (g_background) DeleteObject(g_background);
    return 0;
}
