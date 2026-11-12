#include "машина.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void fail(char*p,size_t n,const char*s){if(p&&n)snprintf(p,n,"%s",s);}
static void assign(ЛексусЗначение*dst,ЛексусЗначение src){лексус_значение_освободить(dst);*dst=src;}
static int store_result(ЛексусЗначение*r,uint8_t dst,ЛексусОшибка e,ЛексусЗначение value,char*err,size_t errn){if(e.код!=ЛЕКСУС_ОК){fail(err,errn,e.сообщение?e.сообщение:"ошибка операции значения");лексус_значение_освободить(&value);return 0;}assign(&r[dst],value);return 1;}
static void cleanup(ЛексусЗначение*r,size_t n){if(r){for(size_t i=0;i<n;++i)лексус_значение_освободить(&r[i]);free(r);}}
int лексус_машина_исполнить(const ЛексусПрограмма*p,uint64_t limit,ЛексусЗначение*out,char*err,size_t errn){
 if(!out){fail(err,errn,"не задано место для результата");return 0;}*out=лексус_ничто();
 if(!лексус_байткод_проверить(p,err,errn))return 0;
 if(!limit)limit=10000000u;
 ЛексусЗначение*r=(ЛексусЗначение*)calloc(p->регистры,sizeof(*r));
 if(!r){fail(err,errn,"не хватает памяти для регистров машины");return 0;}
 for(size_t i=0;i<p->регистры;++i)r[i]=лексус_ничто();
 uint64_t ip=0,steps=0;int done=0,ok=1;
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
   case ЛЕКСУС_ОП_НЕ:value=лексус_логическое(!лексус_правда(r[x->а]));assign(&r[x->назначение],value);++ip;break;
   case ЛЕКСУС_ОП_ПЕРЕХОД:ip=x->операнд;break;
   case ЛЕКСУС_ОП_ПЕРЕХОД_ЕСЛИ_ЛОЖЬ:ip=лексус_правда(r[x->а])?ip+1:x->операнд;break;
   case ЛЕКСУС_ОП_ВОЗВРАТ:*out=лексус_значение_копия(r[x->а]);done=1;break;
   case ЛЕКСУС_ОП_СТОП:done=1;break;
   default:fail(err,errn,"неизвестная инструкция после проверки");ok=0;done=1;break;
  }
 }
 if(ok&&!done){fail(err,errn,"программа завершилась без возврата или остановки");ok=0;}
 cleanup(r,p->регистры);if(!ok)лексус_значение_освободить(out);return ok;
}
