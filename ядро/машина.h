#ifndef ЛЕКСУС_МАШИНА_H
#define ЛЕКСУС_МАШИНА_H
#include "байткод.h"
#include <stddef.h>
#include <stdint.h>
int лексус_машина_исполнить(const ЛексусПрограмма *программа,uint64_t предел_шагов,
                            ЛексусЗначение *результат,char *ошибка,size_t размер_ошибки);
#endif
