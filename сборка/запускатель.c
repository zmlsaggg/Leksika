#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <wchar.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef int (__cdecl *ЛексусPyMain)(int, wchar_t **);

static int добавить(wchar_t *куда, size_t вместимость, const wchar_t *текст)
{
    size_t текущая = wcslen(куда);
    size_t длина = wcslen(текст);
    if (текущая > вместимость || длина >= вместимость - текущая) return 0;
    memcpy(куда + текущая, текст, (длина + 1) * sizeof(wchar_t));
    return 1;
}

static int путь_рядом(wchar_t *куда, size_t вместимость,
                      const wchar_t *каталог, const wchar_t *имя)
{
    if (wcslen(каталог) + 1 + wcslen(имя) + 1 > вместимость) return 0;
    wcscpy(куда, каталог);
    return добавить(куда, вместимость, L"\\") &&
           добавить(куда, вместимость, имя);
}

/* CommandLineToArgvW-compatible escaping for CreateProcessW. */
static int добавить_аргумент(wchar_t *куда, size_t вместимость,
                              const wchar_t *аргумент)
{
    size_t позиция = wcslen(куда);
    size_t слеши = 0;
    if (позиция + 1 >= вместимость) return 0;
    if (позиция) {
        куда[позиция++] = L' ';
        куда[позиция] = 0;
    }
    if (позиция + 1 >= вместимость) return 0;
    куда[позиция++] = L'"';
    for (const wchar_t *p = аргумент; ; ++p) {
        wchar_t c = *p;
        if (c == L'\\') {
            ++слеши;
            continue;
        }
        if (c == L'"') {
            size_t нужно = слеши * 2 + 1;
            if (позиция + нужно + 1 >= вместимость) return 0;
            while (нужно--) куда[позиция++] = L'\\';
            куда[позиция++] = L'"';
            слеши = 0;
            continue;
        }
        if (c == 0) {
            size_t нужно = слеши * 2;
            if (позиция + нужно + 2 >= вместимость) return 0;
            while (нужно--) куда[позиция++] = L'\\';
            куда[позиция++] = L'"';
            куда[позиция] = 0;
            return 1;
        }
        if (позиция + слеши + 2 >= вместимость) return 0;
        while (слеши--) куда[позиция++] = L'\\';
        слеши = 0;
        куда[позиция++] = c;
    }
}

static int запустить_нативное(int argc, wchar_t **argv, const wchar_t *каталог)
{
    wchar_t ядро[32768];
    wchar_t командная_строка[32768] = L"";
    STARTUPINFOW si;
    PROCESS_INFORMATION pi;
    DWORD код = 1;

    if (!путь_рядом(ядро, 32768, каталог, L"лексус-ядро.exe") ||
        GetFileAttributesW(ядро) == INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr, L"Лексика: нативное ядро не найдено рядом с приложением.\n");
        return 127;
    }
    if (!добавить_аргумент(командная_строка, 32768, ядро)) return 2;
    for (int i = 2; i < argc; ++i) {
        if (!добавить_аргумент(командная_строка, 32768, argv[i])) {
            fwprintf(stderr, L"Лексика: слишком длинная командная строка.\n");
            return 2;
        }
    }

    ZeroMemory(&si, sizeof(si));
    ZeroMemory(&pi, sizeof(pi));
    si.cb = sizeof(si);
    if (!CreateProcessW(ядро, командная_строка, NULL, NULL, TRUE,
                        0, NULL, каталог, &si, &pi)) {
        fwprintf(stderr, L"Лексика: не удалось запустить нативное ядро (код %lu).\n",
                 (unsigned long)GetLastError());
        return 2;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    if (!GetExitCodeProcess(pi.hProcess, &код)) код = 1;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)код;
}

int wmain(int argc, wchar_t **argv)
{
    wchar_t исполняемый[32768];
    wchar_t каталог[32768];
    wchar_t dll[32768];
    wchar_t скрипт[32768];
    wchar_t **python_argv = NULL;
    DWORD длина;
    HMODULE runtime = NULL;
    FARPROC entry;
    ЛексусPyMain py_main;
    int результат = 1;

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    if (!GetModuleFileNameW(NULL, исполняемый, (DWORD)(sizeof(исполняемый) / sizeof(исполняемый[0]))) ||
        (длина = (DWORD)wcslen(исполняемый)) == 0 ||
        длина >= sizeof(исполняемый) / sizeof(исполняемый[0])) {
        fwprintf(stderr, L"Лексика: не удалось определить путь приложения.\n");
        return 2;
    }

    wcscpy(каталог, исполняемый);
    wchar_t *последний = wcsrchr(каталог, L'\\');
    if (!последний) {
        fwprintf(stderr, L"Лексика: неверный путь приложения.\n");
        return 2;
    }
    *последний = 0;

    if (argc > 1 && (!_wcsicmp(argv[1], L"нативно") ||
                     !_wcsicmp(argv[1], L"--нативно"))) {
        return запустить_нативное(argc, argv, каталог);
    }

    if (!путь_рядом(dll, 32768, каталог, L"питон\\python312.dll") ||
        !путь_рядом(скрипт, 32768, каталог, L"лексус.lek") ||
        GetFileAttributesW(dll) == INVALID_FILE_ATTRIBUTES ||
        GetFileAttributesW(скрипт) == INVALID_FILE_ATTRIBUTES) {
        fwprintf(stderr, L"Лексика: встроенный runtime или лексус.lek не найден. Переустановите приложение.\n");
        return 2;
    }

    /* Embeddable runtime is isolated by python312._pth next to its DLL.
       Remove user overrides: this launcher must not load another Python. */
    SetEnvironmentVariableW(L"PYTHONHOME", NULL);
    SetEnvironmentVariableW(L"PYTHONPATH", NULL);
    SetEnvironmentVariableW(L"PYTHONUTF8", L"1");
    SetEnvironmentVariableW(L"PYTHONIOENCODING", L"utf-8");

    runtime = LoadLibraryExW(dll, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);
    if (!runtime) {
        fwprintf(stderr, L"Лексика: не удалось загрузить встроенный runtime (код %lu).\n",
                 (unsigned long)GetLastError());
        return 2;
    }
    entry = GetProcAddress(runtime, "Py_Main");
    if (!entry) {
        fwprintf(stderr, L"Лексика: встроенный runtime не экспортирует Py_Main.\n");
        FreeLibrary(runtime);
        return 2;
    }
    py_main = (ЛексусPyMain)(void *)entry;

    /* Same process: Task Manager reports Leksika.exe, not python.exe. */
    python_argv = (wchar_t **)calloc((size_t)argc + 3, sizeof(*python_argv));
    if (!python_argv) {
        fwprintf(stderr, L"Лексика: недостаточно памяти для аргументов.\n");
        FreeLibrary(runtime);
        return 2;
    }
    python_argv[0] = исполняемый;
    python_argv[1] = L"-X";
    python_argv[2] = L"utf8";
    python_argv[3] = скрипт;
    for (int i = 1; i < argc; ++i) python_argv[i + 3] = argv[i];

    результат = py_main(argc + 3, python_argv);
    free(python_argv);
    FreeLibrary(runtime);
    return результат;
}
