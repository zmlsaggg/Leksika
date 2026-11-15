#include "лексус.hpp"
#include <cassert>
#include <string>
#include <utility>

// C callback не должен пропускать C++ исключение через VM.
static int вывод(void *context, const char *data, size_t length) noexcept {
    try { static_cast<std::string *>(context)->append(data, length); return 1; }
    catch (...) { return 0; }
}
int main() {
    std::string output;
    ЛексусСредаИсполнения env{};
    env.контекст = &output;
    env.вывод = вывод;
    env.разрешения = ЛЕКСУС_РАЗРЕШИТЬ_ВЫВОД;
    Лексус::Программа program("пусть элементы равно перечень(1, 2). добавить в элементы 3. сказать длина элементы. вернуть 42.");
    auto result = program.исполнить(env);
    assert(output == "3\n");
    assert(result.данные().вид == ЛЕКСУС_ЧИСЛО);
    auto copy = result;
    auto moved = std::move(copy);
    assert(moved.данные().вид == ЛЕКСУС_ЧИСЛО);
    env.разрешения = 0;
    bool denied = false;
    try { (void)program.исполнить(env); } catch (const std::runtime_error &) { denied = true; }
    assert(denied);
    bool invalid = false;
    try { Лексус::Программа bad("пусть."); } catch (const std::runtime_error &) { invalid = true; }
    assert(invalid);
    bool zero = false;
    try { (void)program.исполнить(env, 0); } catch (const std::invalid_argument &) { zero = true; }
    assert(zero);
    Лексус::Программа loop("пока да повторять сказать 1. конец пока.");
    env.разрешения = ЛЕКСУС_РАЗРЕШИТЬ_ВЫВОД;
    bool limited = false;
    try { (void)loop.исполнить(env, 30); } catch (const std::runtime_error &) { limited = true; }
    assert(limited);
}
