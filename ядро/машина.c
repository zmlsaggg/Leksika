#include "машина.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void fail(char*p,size_t n,const char*s){if(p&&n)snprintf(p,n,"%s",s);}
static void assign(ЛексусЗначение*dst,ЛексусЗначение src){лексус_значение_освободить(dst);*dst=src;}
static int store_result(ЛексусЗначение*r,uint8_t dst,ЛексусОшибка e,ЛексусЗначение value,char*err,size_t errn){if(e.код!=ЛЕКСУС_ОК){fail(err,errn,e.сообщение?e.сообщение:"ошибка операции значения");лексус_значение_освободить(&value);return 0;}assign(&r[dst],value);return 1;}
static void показать_значение(ЛексусЗначение v){
 switch(v.вид){
  case ЛЕКСУС_НИЧТО: fputs("ничто",stdout); break;
  case ЛЕКСУС_ЛОГИКА: fputs(v.данные.логика?"да":"нет",stdout); break;
  case ЛЕКСУС_ЧИСЛО: {
   char *text=NULL;
   ЛексусОшибка e=лексус_число_запись(v,&text);
   if(e.код==ЛЕКСУС_ОК&&text){fputs(text,stdout);free(text);}
   else fputs("ошибка вывода числа",stdout);
   break;
  }
  case ЛЕКСУС_РЕЧЬ: {
   const char *text=NULL; size_t bytes=0;
   ЛексусОшибка e=лексус_речь_текст(v,&text,&bytes);
   if(e.код==ЛЕКСУС_ОК&&text) fwrite(text,1,bytes,stdout);
   else fputs("ошибка вывода речи",stdout);
   break;
  }
  default: fputs(лексус_имя_вида(v.вид),stdout); break;
 }
 putchar('\n');
}
static void cleanup(ЛексусЗначение*r,size_t n){if(r){for(size_t i=0;i<n;++i)лексус_значение_освободить(&r[i]);free(r);}}
typedef struct { ЛексусЗначение остаток; } ЛексусСчётчикЦикла;
static void cleanup_cycles(ЛексусСчётчикЦикла *cycles,size_t count)
{
 if(!cycles) return;
 for(size_t i=0;i<count;++i) лексус_значение_освободить(&cycles[i].остаток);
 free(cycles);
}

int лексус_машина_исполнить(const ЛексусПрограмма*p,uint64_t limit,ЛексусЗначение*out,char*err,size_t errn){
 if(!out){fail(err,errn,"не задано место для результата");return 0;}*out=лексус_ничто();
 if(!лексус_байткод_проверить(p,err,errn))return 0;
 if(!limit)limit=10000000u;
 ЛексусЗначение*r=(ЛексусЗначение*)calloc(p->регистры,sizeof(*r));
 if(!r){fail(err,errn,"не хватает памяти для регистров машины");return 0;}
 for(size_t i=0;i<p->регистры;++i)r[i]=лексус_ничто();
 uint64_t ip=0,steps=0;int done=0,ok=1;
 ЛексусСчётчикЦикла *cycles=NULL;
 size_t cycles_count=0,cycles_capacity=0;
 while(ip<p->число_инструкций&&!done){
  if(++steps>limit){fail(err,errn,"превышен предел шагов виртуальной машины");ok=0;break;}
  const ЛексусИнструкция*x=&p->код[ip];ЛексусЗначение value=лексус_ничто();ЛексусОшибка e={ЛЕКСУС_ОК,NULL};
  switch(x->операция){
   case ЛЕКСУС_ОП_КОНСТАНТА:value=лексус_значение_копия(p->константы[x->операнд]);assign(&r[x->назначение],value);++ip;break;
   case ЛЕКСУС_ОП_КОПИЯ:value=лексус_значение_копия(r[x->а]);assign(&r[x->назначение],value);++ip;break;
   case ЛЕКСУС_ОП_СЛОЖИТЬ:
    e=лексус_сложить(r[x->а],r[x->б],&value);
    if(!store_result(r,x->назначение,e,value,err,errn)){ok=0;done=1;}else ++ip;
    break;
   case ЛЕКСУС_ОП_ВЫЧЕСТЬ:
    e=лексус_число_вычесть(r[x->а],r[x->б],&value);
    if(!store_result(r,x->назначение,e,value,err,errn)){ok=0;done=1;}else ++ip;
    break;
   case ЛЕКСУС_ОП_УМНОЖИТЬ:
    e=лексус_число_умножить(r[x->а],r[x->б],&value);
    if(!store_result(r,x->назначение,e,value,err,errn)){ok=0;done=1;}else ++ip;
    break;
   case ЛЕКСУС_ОП_РАЗДЕЛИТЬ:
    e=лексус_число_разделить(r[x->а],r[x->б],&value);
    if(!store_result(r,x->назначение,e,value,err,errn)){ok=0;done=1;}else ++ip;
    break;
   case ЛЕКСУС_ОП_РАВНО: {
    int equal=0;
    e=лексус_равно(r[x->а],r[x->б],&equal);
    if(e.код==ЛЕКСУС_ОК)value=лексус_логическое(equal);
    if(!store_result(r,x->назначение,e,value,err,errn)){ok=0;done=1;}else ++ip;
    break;
   }
   case ЛЕКСУС_ОП_ЦЕЛОЕ:
    e=лексус_число_целое(r[x->а],&value);
    if(!store_result(r,x->назначение,e,value,err,errn)){ok=0;done=1;}else ++ip;
    break;
   case ЛЕКСУС_ОП_ПОВТОРИТЬ_НАЧАЛО: {
    if(r[x->а].вид!=ЛЕКСУС_ЧИСЛО) {
     fail(err,errn,"команда «повторить» требует числовое количество повторений");
     ok=0;done=1;break;
    }
    ЛексусЗначение zero=лексус_ничто();
    e=лексус_число_создать("0",&zero);
    int comparison=0;
    if(e.код==ЛЕКСУС_ОК) e=лексус_число_сравнить(r[x->а],zero,&comparison);
    лексус_значение_освободить(&zero);
    if(e.код!=ЛЕКСУС_ОК) {
     fail(err,errn,e.сообщение?e.сообщение:"не удалось проверить количество повторений");
     ok=0;done=1;break;
    }
    if(comparison<=0) { ip=x->операнд;break; }
    if(cycles_count==cycles_capacity) {
     size_t next=cycles_capacity?cycles_capacity*2:8;
     if(next<cycles_capacity || next>SIZE_MAX/sizeof(*cycles)) {
      fail(err,errn,"слишком много вложенных циклов «повторить»");ok=0;done=1;break;
     }
     ЛексусСчётчикЦикла *grown=(ЛексусСчётчикЦикла*)realloc(cycles,next*sizeof(*cycles));
     if(!grown) { fail(err,errn,"не хватает памяти для стека циклов");ok=0;done=1;break; }
     cycles=grown;cycles_capacity=next;
    }
    cycles[cycles_count++].остаток=лексус_значение_копия(r[x->а]);
    ++ip;break;
   }
   case ЛЕКСУС_ОП_ПОВТОРИТЬ_ПРЕРВАТЬ: {
    if(!cycles_count) { fail(err,errn,"выход из «повторить» без соответствующего начала цикла");ok=0;done=1;break; }
    лексус_значение_освободить(&cycles[--cycles_count].остаток);
    ip=x->операнд;
    break;
   }
   case ЛЕКСУС_ОП_ПОВТОРИТЬ_ШАГ: {
    if(!cycles_count) { fail(err,errn,"шаг «повторить» без соответствующего начала цикла");ok=0;done=1;break; }
    ЛексусЗначение one=лексус_ничто(),zero=лексус_ничто(),next=лексус_ничто();
    e=лексус_число_создать("1",&one);
    if(e.код==ЛЕКСУС_ОК)
     e=лексус_число_вычесть(cycles[cycles_count-1].остаток,one,&next);
    if(e.код==ЛЕКСУС_ОК) e=лексус_число_создать("0",&zero);
    int comparison=0;
    if(e.код==ЛЕКСУС_ОК) e=лексус_число_сравнить(next,zero,&comparison);
    лексус_значение_освободить(&one);
    лексус_значение_освободить(&zero);
    if(e.код!=ЛЕКСУС_ОК) {
     лексус_значение_освободить(&next);
     fail(err,errn,e.сообщение?e.сообщение:"ошибка счётчика цикла");
     ok=0;done=1;break;
    }
    assign(&cycles[cycles_count-1].остаток,next);
    if(comparison>0) ip=x->операнд;
    else {
     лексус_значение_освободить(&cycles[--cycles_count].остаток);
     ++ip;
    }
    break;
   }
   case ЛЕКСУС_ОП_БОЛЬШЕ:
   case ЛЕКСУС_ОП_МЕНЬШЕ:
   case ЛЕКСУС_ОП_НЕБОЛЬШЕ:
   case ЛЕКСУС_ОП_НЕМЕНЬШЕ: {
    int comparison=0;
    e=лексус_число_сравнить(r[x->а],r[x->б],&comparison);
    if(e.код==ЛЕКСУС_ОК) {
     int answer=x->операция==ЛЕКСУС_ОП_БОЛЬШЕ?comparison>0:
                x->операция==ЛЕКСУС_ОП_МЕНЬШЕ?comparison<0:
                x->операция==ЛЕКСУС_ОП_НЕБОЛЬШЕ?comparison<=0:comparison>=0;
     value=лексус_логическое(answer);
    }
    if(!store_result(r,x->назначение,e,value,err,errn)){ok=0;done=1;}else ++ip;
    break;
   }
   case ЛЕКСУС_ОП_И:
   case ЛЕКСУС_ОП_ИЛИ:
    value=лексус_логическое(x->операция==ЛЕКСУС_ОП_И?
      (лексус_правда(r[x->а])&&лексус_правда(r[x->б])):
      (лексус_правда(r[x->а])||лексус_правда(r[x->б])));
    assign(&r[x->назначение],value);++ip;break;
   case ЛЕКСУС_ОП_НЕ:value=лексус_логическое(!лексус_правда(r[x->а]));assign(&r[x->назначение],value);++ip;break;
   case ЛЕКСУС_ОП_ПЕРЕХОД:ip=x->операнд;break;
   case ЛЕКСУС_ОП_ПЕРЕХОД_ЕСЛИ_ЛОЖЬ:ip=лексус_правда(r[x->а])?ip+1:x->операнд;break;
   case ЛЕКСУС_ОП_ВОЗВРАТ:*out=лексус_значение_копия(r[x->а]);done=1;break;
   case ЛЕКСУС_ОП_СТОП:done=1;break;
   case ЛЕКСУС_ОП_ПОКАЗАТЬ:показать_значение(r[x->назначение]);++ip;break;
   default:fail(err,errn,"неизвестная инструкция после проверки");ok=0;done=1;break;
  }
 }
 if(ok&&!done){fail(err,errn,"программа завершилась без возврата или остановки");ok=0;}
 cleanup_cycles(cycles,cycles_count);
 cleanup(r,p->регистры);if(!ok)лексус_значение_освободить(out);return ok;
}
