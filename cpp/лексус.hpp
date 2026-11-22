#ifndef ЛЕКСУС_CPP_HPP
#define ЛЕКСУС_CPP_HPP

// Необязательная C++-обёртка для проверяемого нативного подмножества.
// Семантика и права доступа остаются в C API/VM, здесь только владение ресурсами.
#include "../ядро/компилятор.h"
#include "../ядро/машина.h"
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <cstdint>

namespace Лексус {
class Значение {
public:
    explicit Значение(ЛексусЗначение value) noexcept : value_(value) {}
    ~Значение() { лексус_значение_освободить(&value_); }
    Значение(const Значение &other) : value_(лексус_значение_копия(other.value_)) {}
    Значение(Значение &&other) noexcept : value_(other.value_) { other.value_ = лексус_ничто(); }
    Значение &operator=(Значение other) noexcept { std::swap(value_, other.value_); return *this; }
    const ЛексусЗначение &данные() const noexcept { return value_; }
private:
    ЛексусЗначение value_;
};

class Программа {
public:
    explicit Программа(std::string_view источник, unsigned профили = ЛЕКСУС_ПРОФИЛЬ_БЕЗ_ИЗМЕНЕНИЙ)
        : compiled_{} {
        char error[512] = {};
        if (!лексус_скомпилировать_с_профилями(источник.data(), источник.size(), профили,
                                                 &compiled_, error, sizeof(error))) {
            throw std::runtime_error(error[0] ? error : "Не удалось скомпилировать программу");
        }
    }
    ~Программа() { лексус_скомпиляцию_освободить(&compiled_); }
    Программа(const Программа &) = delete;
    Программа &operator=(const Программа &) = delete;
    Программа(Программа &&other) noexcept : compiled_(other.compiled_) {
        other.compiled_ = {};
    }
    Программа &operator=(Программа &&other) noexcept {
        if (this != &other) {
            лексус_скомпиляцию_освободить(&compiled_);
            compiled_ = other.compiled_;
            other.compiled_ = {};
        }
        return *this;
    }

    // Среда обязательна: вызывающая программа явно задаёт разрешения VM.
    Значение исполнить(const ЛексусСредаИсполнения &среда, std::uint64_t предел_шагов = 1000000) const {
        if (!предел_шагов) throw std::invalid_argument("Предел шагов должен быть положительным");
        ЛексусПрограмма program{compiled_.код, compiled_.число_инструкций,
            compiled_.константы, compiled_.число_констант, compiled_.регистры,
            compiled_.функции, compiled_.число_функций, compiled_.вызовы, compiled_.число_вызовов,
            compiled_.позиции, compiled_.число_позиций};
        char error[512] = {};
        ЛексусЗначение result = лексус_ничто();
        if (!лексус_машина_исполнить_со_средой(&program, предел_шагов, &среда, &result, error, sizeof(error))) {
            лексус_значение_освободить(&result);
            throw std::runtime_error(error[0] ? error : "Не удалось исполнить программу");
        }
        return Значение(result);
    }
private:
    ЛексусСкомпилированнаяПрограмма compiled_;
};
}
#endif
