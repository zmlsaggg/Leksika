#include "машина.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static void fail(char*p,size_t n,const char*s){if(p&&n)snprintf(p,n,"%s",s);}
static void assign(ЛексусЗначение*dst,ЛексусЗначение src){лексус_значение_освободить(dst);*dst=src;}
static int store_result(ЛексусЗначение*r,uint8_t dst,ЛексусОшибка e,ЛексусЗначение value,char*err,size_t errn){if(e.код!=ЛЕКСУС_ОК){fail(err,errn,e.сообщение?e.сообщение:"ошибка операции значения");лексус_значение_освободить(&value);return 0;}assign(&r[dst],value);return 1;}
static int вывести_байты(const ЛексусСредаИсполнения *env,const char *bytes,size_t length)
{
 if(env&&env->вывод) return env->вывод(env->контекст,bytes,length)==1;
 return fwrite(bytes,1,length,stdout)==length;
}
static int показать_значение(ЛексусЗначение v,const ЛексусСредаИсполнения *env,char *err,size_t errn)
{
 const char *text=NULL; size_t length=0; char *owned=NULL;
 switch(v.вид){
  case ЛЕКСУС_НИЧТО: text="ничто"; length=strlen(text); break;
  case ЛЕКСУС_ЛОГИКА: text=v.данные.логика?"да":"нет"; length=strlen(text); break;
  case ЛЕКСУС_ЧИСЛО: {
   ЛексусОшибка e=лексус_число_запись(v,&owned);
   if(e.код!=ЛЕКСУС_ОК||!owned){fail(err,errn,e.сообщение?e.сообщение:"не удалось преобразовать число для вывода");return 0;}
   text=owned; length=strlen(owned); break;
  }
  case ЛЕКСУС_РЕЧЬ: {
   ЛексусОшибка e=лексус_речь_текст(v,&text,&length);
   if(e.код!=ЛЕКСУС_ОК||!text){fail(err,errn,e.сообщение?e.сообщение:"не удалось получить текст речи");return 0;}
   break;
  }
  default: fail(err,errn,"этот вид значения пока нельзя вывести как речь"); return 0;
 }
 int ok=вывести_байты(env,text,length)&&вывести_байты(env,"\n",1);
 free(owned);
 if(!ok) fail(err,errn,"среда отклонила вывод значения");
 return ok;
}
static int ввод_по_умолчанию(void *context,char *buffer,size_t capacity,size_t *length)
{
 (void)context;
 if(!buffer||!capacity||!length) return -1;
 size_t used=0; int ch;
 while((ch=getchar())!=EOF&&ch!='\n'){
  if(used+1>=capacity){while(ch!='\n'&&(ch=getchar())!=EOF){}return -1;}
  buffer[used++]=(char)ch;
 }
 if(ch==EOF&&used==0) return 0;
 if(used&&buffer[used-1]=='\r') --used;
 *length=used; return 1;
}
static void cleanup(ЛексусЗначение*r,size_t n){if(r){for(size_t i=0;i<n;++i)лексус_значение_освободить(&r[i]);free(r);}}
typedef struct { ЛексусЗначение остаток; } ЛексусСчётчикЦикла;
typedef struct { ЛексусЗначение перечень; size_t индекс; uint32_t выход; } ЛексусИтератор;
static void cleanup_cycles(ЛексусСчётчикЦикла *cycles,size_t count)
{
 if(!cycles) return;
 for(size_t i=0;i<count;++i) лексус_значение_освободить(&cycles[i].остаток);
 free(cycles);
}
static void cleanup_iterators(ЛексусИтератор *items,size_t count)
{
 if(!items) return;
 for(size_t i=0;i<count;++i) лексус_значение_освободить(&items[i].перечень);
 free(items);
}

int лексус_машина_исполнить_со_средой(const ЛексусПрограмма*p,uint64_t limit,
                                      const ЛексусСредаИсполнения *env,
                                      ЛексусЗначение*out,char*err,size_t errn){
 if(!out){fail(err,errn,"не задано место для результата");return 0;}*out=лексус_ничто();
 if(!лексус_байткод_проверить(p,err,errn))return 0;
 if(!limit)limit=10000000u;
 ЛексусЗначение*r=(ЛексусЗначение*)calloc(p->регистры,sizeof(*r));
 if(!r){fail(err,errn,"не хватает памяти для регистров машины");return 0;}
 for(size_t i=0;i<p->регистры;++i)r[i]=лексус_ничто();
 uint64_t ip=0,steps=0;int done=0,ok=1;
 ЛексусСчётчикЦикла *cycles=NULL;
 size_t cycles_count=0,cycles_capacity=0;
 ЛексусИтератор *iterators=NULL;
 size_t iterators_count=0,iterators_capacity=0;
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
   case ЛЕКСУС_ОП_ПОКАЗАТЬ:
    if(!показать_значение(r[x->назначение],env,err,errn)){ok=0;done=1;}else ++ip;
    break;
   case ЛЕКСУС_ОП_ПЕРЕЧЕНЬ_СОЗДАТЬ: {
    value=лексус_ничто(); e=лексус_перечень_создать(&value);
    if(!store_result(r,x->назначение,e,value,err,errn)){ok=0;done=1;}else ++ip;
    break;
   }
   case ЛЕКСУС_ОП_ПЕРЕЧЕНЬ_ДОБАВИТЬ:
    e=лексус_перечень_добавить(r[x->а],r[x->б]);
    if(e.код!=ЛЕКСУС_ОК){fail(err,errn,e.сообщение?e.сообщение:"не удалось добавить элемент в перечень");ok=0;done=1;}else ++ip;
    break;
   case ЛЕКСУС_ОП_ИТЕРАЦИЯ_НАЧАТЬ: {
    size_t count=0;
    e=лексус_перечень_длина(r[x->а],&count);
    if(e.код!=ЛЕКСУС_ОК){fail(err,errn,e.сообщение?e.сообщение:"обход требует перечень");ok=0;done=1;break;}
    if(!count){ip=x->операнд;break;}
    if(iterators_count==iterators_capacity){
     size_t next=iterators_capacity?iterators_capacity*2:8;
     if(next<iterators_capacity||next>SIZE_MAX/sizeof(*iterators)){fail(err,errn,"слишком много вложенных обходов перечней");ok=0;done=1;break;}
     ЛексусИтератор *grown=(ЛексусИтератор*)realloc(iterators,next*sizeof(*iterators));
     if(!grown){fail(err,errn,"не хватает памяти для стека итераторов");ok=0;done=1;break;}
     iterators=grown;iterators_capacity=next;
    }
    iterators[iterators_count].перечень=лексус_значение_копия(r[x->а]);
    iterators[iterators_count].индекс=0;
    iterators[iterators_count].выход=x->операнд;
    ++iterators_count; ++ip; break;
   }
   case ЛЕКСУС_ОП_ИТЕРАЦИЯ_ШАГ: {
    if(!iterators_count){fail(err,errn,"шаг обхода без соответствующего начала");ok=0;done=1;break;}
    ЛексусИтератор *iterator=&iterators[iterators_count-1];
    size_t count=0;
    e=лексус_перечень_длина(iterator->перечень,&count);
    if(e.код!=ЛЕКСУС_ОК){fail(err,errn,e.сообщение?e.сообщение:"не удалось прочитать длину перечня");ok=0;done=1;break;}
    if(iterator->индекс>=count){uint32_t target=iterator->выход;лексус_значение_освободить(&iterator->перечень);--iterators_count;ip=target;break;}
    value=лексус_ничто();
    e=лексус_перечень_получить(iterator->перечень,iterator->индекс++,&value);
    if(!store_result(r,x->назначение,e,value,err,errn)){ok=0;done=1;}else ip=x->операнд;
    break;
   }
   case ЛЕКСУС_ОП_ИТЕРАЦИЯ_ПРЕРВАТЬ:
    if(!iterators_count){fail(err,errn,"выход из обхода без соответствующего начала");ok=0;done=1;break;}
    лексус_значение_освободить(&iterators[--iterators_count].перечень);
    ip=x->операнд; break;
   case ЛЕКСУС_ОП_ПРОВЕРИТЬ: {
    if(!лексус_правда(r[x->а])) {
     const char *reason=NULL; size_t reason_length=0;
     e=лексус_речь_текст(r[x->б],&reason,&reason_length);
     if(e.код!=ЛЕКСУС_ОК||!reason) fail(err,errn,e.сообщение?e.сообщение:"условие не выполнено");
     else if(err&&errn) {
      size_t limit=reason_length<errn-1?reason_length:errn-1;
      size_t copy=0;
      while(copy<limit&&reason[copy]!='\0') ++copy;
      while(copy>0&&copy<reason_length&&(((unsigned char)reason[copy]&0xC0u)==0x80u)) --copy;
      memcpy(err,reason,copy); err[copy]='\0';
     }
     ok=0;done=1;
    } else ++ip;
    break;
   }
   case ЛЕКСУС_ОП_ВВОД: {
    const size_t capacity=65536u;
    char *buffer=(char*)malloc(capacity);
    if(!buffer){fail(err,errn,"не хватает памяти для ввода");ok=0;done=1;break;}
    size_t length=0;
    ЛексусВводСреды read_line=env&&env->ввод?env->ввод:ввод_по_умолчанию;
    int read_status=read_line(env?env->контекст:NULL,buffer,capacity,&length);
    if(read_status!=1||length>=capacity){
     free(buffer);
     fail(err,errn,read_status==0?"достигнут конец ввода":"среда не смогла предоставить строку ввода");
     ok=0;done=1;break;
    }
    ЛексусЗначение input=лексус_ничто();
    e=лексус_речь_создать(buffer,length,&input);
    free(buffer);
    if(!store_result(r,x->назначение,e,input,err,errn)){ok=0;done=1;}else ++ip;
    break;
   }
   default:fail(err,errn,"неизвестная инструкция после проверки");ok=0;done=1;break;
  }
 }
 if(ok&&!done){fail(err,errn,"программа завершилась без возврата или остановки");ok=0;}
 cleanup_cycles(cycles,cycles_count);
 cleanup_iterators(iterators,iterators_count);
 cleanup(r,p->регистры);if(!ok)лексус_значение_освободить(out);return ok;
}
int лексус_машина_исполнить(const ЛексусПрограмма*p,uint64_t limit,ЛексусЗначение*out,char*err,size_t errn)
{
 return лексус_машина_исполнить_со_средой(p,limit,NULL,out,err,errn);
}
