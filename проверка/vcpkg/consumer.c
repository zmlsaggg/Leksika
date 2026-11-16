#include <leksika/ядро/компилятор.h>
#include <leksika/ядро/машина.h>
#include <string.h>

int main(void) {
    const char *source = "вернуть 42.";
    char error[512] = {0};
    ЛексусСкомпилированнаяПрограмма compiled = {0};
    if (!лексус_скомпилировать(source, strlen(source), &compiled, error, sizeof(error))) return 1;
    ЛексусПрограмма program = {compiled.код, compiled.число_инструкций, compiled.константы,
        compiled.число_констант, compiled.регистры, compiled.функции, compiled.число_функций,
        compiled.вызовы, compiled.число_вызовов};
    ЛексусСредаИсполнения env = {0};
    ЛексусЗначение result = лексус_ничто();
    int ok = лексус_машина_исполнить_со_средой(&program, 1000000, &env, &result, error, sizeof(error));
    ЛексусЗначение expected = лексус_ничто();
    ЛексусОшибка err = лексус_число_создать("42", &expected);
    int equal = 0;
    if (ok && err.код == ЛЕКСУС_ОК) err = лексус_равно(result, expected, &equal);
    int valid = ok && err.код == ЛЕКСУС_ОК && equal;
    лексус_значение_освободить(&expected);
    лексус_значение_освободить(&result);
    лексус_скомпиляцию_освободить(&compiled);
    return valid ? 0 : 2;
}
