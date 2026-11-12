#ifndef ЛЕКСУС_СЛОВАРЬ_ИНДЕКС_H
#define ЛЕКСУС_СЛОВАРЬ_ИНДЕКС_H
#include <stddef.h>
#include <stdint.h>
typedef struct {
    unsigned char *файл;
    const unsigned char *смещения;
    const unsigned char *данные;
    uint64_t количество;
    uint64_t размер_данных;
} ЛексусИндекс;
int лексус_индекс_открыть(ЛексусИндекс *индекс, const char *путь, char *ошибка, size_t размер_ошибки);
void лексус_индекс_закрыть(ЛексусИндекс *индекс);
int лексус_индекс_найти(const ЛексусИндекс *индекс, const char *форма);
size_t лексус_индекс_подсказать(const ЛексусИндекс *индекс, const char *начало, const char **результат, size_t предел);
#endif
