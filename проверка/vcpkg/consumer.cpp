#include <leksika/cpp/лексус.hpp>
#include <stdexcept>

int main() {
    try {
        ЛексусСредаИсполнения env{};
        Лексус::Программа program("вернуть 42.");
        auto result = program.исполнить(env);
        ЛексусЗначение expected = лексус_ничто();
        ЛексусОшибка err = лексус_число_создать("42", &expected);
        int equal = 0;
        if (err.код == ЛЕКСУС_ОК) err = лексус_равно(result.данные(), expected, &equal);
        лексус_значение_освободить(&expected);
        return err.код == ЛЕКСУС_ОК && equal ? 0 : 1;
    } catch (const std::exception &) { return 2; }
}
