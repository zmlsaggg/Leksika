/*
 * Лексус — нативное ядро: UTF-8, лексический анализ, ограниченный компилятор .lek и VM.
 * C11 и стандартная библиотека используются как временный bootstrap.
 * Полная грамматика языка пока остаётся в переходной реализации.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "словарь_формы.h"
#include "словарь_индекс.h"
#include "компилятор.h"
#include "машина.h"

#define ЛЕКСУС_ВЕРСИЯ "0.6.0-native"
typedef struct { size_t знаки, строки; } Счётчик;
typedef struct { const unsigned char *текст; size_t длина, позиция, строка, столбец; } Сканер;
typedef struct { const char *вид; size_t начало, длина, строка, столбец; } Токен;

static int utf8_код(const unsigned char *s, size_t n, size_t *ширина, uint32_t *код)
{
    if (!n) return 0;
    unsigned char a=s[0]; unsigned k; uint32_t c;
    if (a<0x80) { c=a; k=1; }
    else if (a>=0xC2 && a<=0xDF) { c=a&0x1F; k=2; }
    else if (a>=0xE0 && a<=0xEF) { c=a&0x0F; k=3; }
    else if (a>=0xF0 && a<=0xF4) { c=a&0x07; k=4; }
    else return 0;
    if (k>n) return 0;
    for (unsigned i=1;i<k;++i) {
        unsigned char b=s[i];
        if ((b&0xC0)!=0x80) return 0;
        if (i==1) {
            if (a==0xE0 && b<0xA0) return 0;
            if (a==0xED && b>=0xA0) return 0;
            if (a==0xF0 && b<0x90) return 0;
            if (a==0xF4 && b>=0x90) return 0;
        }
        c=(c<<6)|(b&0x3F);
    }
    if (c>0x10FFFF || (c>=0xD800 && c<=0xDFFF)) return 0;
    *ширина=k; *код=c; return 1;
}
static int utf8_проверить(const unsigned char *d,size_t n,Счётчик *с)
{
    size_t i=0; Счётчик итог={0,1};
    while(i<n) { size_t w; uint32_t c; if(!utf8_код(d+i,n-i,&w,&c)) return 0;
        i+=w; ++итог.знаки; if(c==10) ++итог.строки; }
    if(с) { *с=итог; }
    return 1;
}

#include "морфология_окончания.h"

typedef struct {
    char основа[512];
    const char *падеж;
    const char *число;
} МорфологическийВариант;

static size_t символы_utf8(const char *текст)
{
    size_t n=strlen(текст),i=0,счёт=0;
    while(i<n) { size_t w; uint32_t c; if(!utf8_код((const unsigned char*)текст+i,n-i,&w,&c)) break; i+=w; ++счёт; }
    return счёт;
}
static int русское_нижнее(const char *вход,char *выход,size_t размер)
{
    size_t n=strlen(вход),i=0,o=0;
    while(i<n) {
        size_t w; uint32_t c;
        if(!utf8_код((const unsigned char*)вход+i,n-i,&w,&c)) return 0;
        if(c>=0x0410 && c<=0x042F) c+=0x20;
        else if(c==0x0401) c=0x0451;
        else if(c>='A' && c<='Z') c+=0x20;
        unsigned char b[4]; size_t k;
        if(c<0x80) { b[0]=(unsigned char)c; k=1; }
        else if(c<0x800) { b[0]=(unsigned char)(0xC0|(c>>6)); b[1]=(unsigned char)(0x80|(c&0x3F)); k=2; }
        else if(c<0x10000) { b[0]=(unsigned char)(0xE0|(c>>12)); b[1]=(unsigned char)(0x80|((c>>6)&0x3F)); b[2]=(unsigned char)(0x80|(c&0x3F)); k=3; }
        else { b[0]=(unsigned char)(0xF0|(c>>18)); b[1]=(unsigned char)(0x80|((c>>12)&0x3F)); b[2]=(unsigned char)(0x80|((c>>6)&0x3F)); b[3]=(unsigned char)(0x80|(c&0x3F)); k=4; }
        if(o+k>=размер) return 0;
        memcpy(выход+o,b,k); o+=k; i+=w;
    }
    выход[o]='\0'; return 1;
}
#include "морфология_имён.h"

static int добавить_вариант(МорфологическийВариант *массив,size_t *число,size_t максимум,
                            const char *основа,const char *падеж,const char *количество)
{
    for(size_t i=0;i<*число;++i)
        if(strcmp(массив[i].основа,основа)==0 && strcmp(массив[i].падеж,падеж)==0 &&
           strcmp(массив[i].число,количество)==0) return 1;
    if(*число>=максимум || strlen(основа)>=sizeof(массив[0].основа)) return 0;
    strcpy(массив[*число].основа,основа);
    массив[*число].падеж=падеж; массив[*число].число=количество; ++*число;
    return 1;
}
static size_t морфология_разобрать(const char *исходное,МорфологическийВариант *результат,size_t максимум)
{
    char слово[512];
    if(!русское_нижнее(исходное,слово,sizeof(слово))) return 0;
    size_t длина=strlen(слово),число=0;
    size_t окончаний=sizeof(ОКОНЧАНИЯ_РАЗБОРА)/sizeof(ОКОНЧАНИЯ_РАЗБОРА[0]);
    for(size_t i=0;i<окончаний;++i) {
        const char *ок=ОКОНЧАНИЯ_РАЗБОРА[i].окончание;
        size_t n=strlen(ок);
        if(длина<=n || memcmp(слово+длина-n,ок,n)!=0) continue;
        char основа[512]; size_t остаток=длина-n;
        memcpy(основа,слово,остаток); основа[остаток]='\0';
        if(символы_utf8(основа)<2) continue;
        if(!добавить_вариант(результат,&число,максимум,основа,
                             ОКОНЧАНИЯ_РАЗБОРА[i].падеж,ОКОНЧАНИЯ_РАЗБОРА[i].число)) break;
    }
    добавить_вариант(результат,&число,максимум,слово,"им","ед");
    return число;
}
static const char *название_падежа(const char *код)
{
    if(strcmp(код,"им")==0) return "именительный";
    if(strcmp(код,"рд")==0) return "родительный";
    if(strcmp(код,"дт")==0) return "дательный";
    if(strcmp(код,"вн")==0) return "винительный";
    if(strcmp(код,"тв")==0) return "творительный";
    return "предложный";
}

static int кириллица(uint32_t c)
{
    return (c>=0x0400&&c<=0x052F)||(c>=0x2DE0&&c<=0x2DFF)||(c>=0xA640&&c<=0xA69F);
}
static int латиница(uint32_t c) { return (c>='A'&&c<='Z')||(c>='a'&&c<='z'); }
static int буква(uint32_t c) { return кириллица(c)||латиница(c); }
static int цифра(uint32_t c) { return c>='0'&&c<='9'; }
static int взять(Сканер *s,uint32_t *c,size_t *w)
{
    if(s->позиция>=s->длина) return 0;
    return utf8_код(s->текст+s->позиция,s->длина-s->позиция,w,c)?1:-1;
}
static void шаг(Сканер *s,size_t w,uint32_t c)
{
    s->позиция+=w; if(c==10) { ++s->строка; s->столбец=1; } else ++s->столбец;
}
static int совпадает(const Сканер *s,const char *слово)
{
    size_t n=strlen(слово);
    return s->позиция+n<=s->длина && memcmp(s->текст+s->позиция,слово,n)==0;
}
static int граница_слова(const Сканер *s,size_t p)
{
    if(p>=s->длина) return 1;
    size_t w; uint32_t c;
    if(!utf8_код(s->текст+p,s->длина-p,&w,&c)) return 0;
    return !буква(c)&&!цифра(c)&&c!='-';
}

/* 1 — токен, 0 — конец, -1 — ошибка. */
static int следующий(Сканер *s,Токен *t,char *ошибка,size_t размер)
{
    while(s->позиция<s->длина) {
        size_t w; uint32_t c; int r=взять(s,&c,&w);
        if(r<0) { snprintf(ошибка,размер,"некорректный UTF-8: строка %zu, столбец %zu",s->строка,s->столбец); return -1; }
        if(c==' '||c=='\t'||c=='\r'||c=='\n') { шаг(s,w,c); continue; }
        break;
    }
    if(s->позиция>=s->длина) return 0;
    t->начало=s->позиция; t->строка=s->строка; t->столбец=s->столбец;
    size_t w; uint32_t c;
    if(взять(s,&c,&w)<0) { snprintf(ошибка,размер,"некорректный UTF-8"); return -1; }

    if(совпадает(s,"речь") && граница_слова(s,s->позиция+strlen("речь"))) {
        s->позиция+=strlen("речь"); s->столбец+=4;
        unsigned глубина=0;
        while(s->позиция<s->длина) {
            if(глубина==0 && совпадает(s,"конец") && граница_слова(s,s->позиция+strlen("конец"))) {
                size_t p=s->позиция+strlen("конец");
                while(p<s->длина&&(s->текст[p]==' '||s->текст[p]=='\t')) ++p;
                if(p+strlen("речи")<=s->длина && memcmp(s->текст+p,"речи",strlen("речи"))==0 &&
                   граница_слова(s,p+strlen("речи"))) {
                    size_t после=p+strlen("речи");
                    while(s->позиция<после) { size_t sw; uint32_t sc; if(взять(s,&sc,&sw)<=0) break; шаг(s,sw,sc); }
                    t->вид="речь"; t->длина=s->позиция-t->начало; return 1;
                }
            }
            if(взять(s,&c,&w)<=0) break;
            if(c=='(') ++глубина; else if(c==')'&&глубина) --глубина;
            шаг(s,w,c);
        }
        snprintf(ошибка,размер,"речь не закрыта: строка %zu, столбец %zu; ожидается «конец речи»",t->строка,t->столбец);
        return -1;
    }
    if(цифра(c)) {
        while(s->позиция<s->длина) { if(взять(s,&c,&w)<=0||!цифра(c)) break; шаг(s,w,c); }
        t->вид="число"; t->длина=s->позиция-t->начало; return 1;
    }
    if(буква(c)) {
        int была_латиница=0;
        while(s->позиция<s->длина) {
            if(взять(s,&c,&w)<=0) break;
            if(буква(c)) { if(латиница(c)) была_латиница=1; шаг(s,w,c); continue; }
            if(c=='-'&&s->позиция+w<s->длина) {
                size_t nw; uint32_t nc;
                if(utf8_код(s->текст+s->позиция+w,s->длина-s->позиция-w,&nw,&nc)&&буква(nc)) { шаг(s,w,c); continue; }
            }
            break;
        }
        t->длина=s->позиция-t->начало;
        if(была_латиница) { snprintf(ошибка,размер,"латинское слово запрещено: строка %zu, столбец %zu",t->строка,t->столбец); return -1; }
        t->вид="слово"; return 1;
    }
    if(c=='.'||c==','||c=='!'||c=='?'||c=='('||c==')') {
        шаг(s,w,c); t->вид="знак"; t->длина=w; return 1;
    }
    snprintf(ошибка,размер,"знак или символ запрещён: строка %zu, столбец %zu",s->строка,s->столбец);
    return -1;
}
static int разобрать_поток(const unsigned char *d,size_t n,int печатать,int сообщать,const ЛексусИндекс *индекс,size_t *число)
{
    Сканер s={d,n,0,1,1}; size_t всего=0; char ошибка[256];
    for(;;) {
        Токен t; int r=следующий(&s,&t,ошибка,sizeof(ошибка));
        if(r==0) break;
        if(r<0) { if(сообщать) fprintf(stderr,"Лексус: %s.\n",ошибка); return 1; }
        ++всего;
        if(печатать) {
            printf("%4zu:%-3zu %-7s ",t.строка,t.столбец,t.вид);
            fwrite(d+t.начало,1,t.длина,stdout);
            if(strcmp(t.вид,"слово")==0 && t.длина<512) {
                char форма[512];
                memcpy(форма,d+t.начало,t.длина); форма[t.длина]='\0';
                const char *лемма=найти_служебную_лемму(форма);
                if(лемма) printf("  (лемма: %s)",лемма);
                if(индекс) {
                    char нормализованная[512];
                    if(русское_нижнее(форма,нормализованная,sizeof(нормализованная)) &&
                       лексус_индекс_найти(индекс,нормализованная))
                        printf("  (словарная форма)");
                }
                МорфологическийВариант варианты[32];
                size_t найдено=морфология_разобрать(форма,варианты,32);
                if(найдено) {
                    printf("  (морфология:");
                    for(size_t номер=0;номер<найдено;++номер)
                        printf(" %s:%s/%s",варианты[номер].основа,
                               название_падежа(варианты[номер].падеж),варианты[номер].число);
                    putchar(')');
                }
            }
            putchar('\n');
        }
    }
    if(число) { *число=всего; }
    return 0;
}
static unsigned char *прочитать(FILE *f,size_t *длина)
{
    size_t размер=4096,n=0; unsigned char *b=(unsigned char*)malloc(размер);
    if(!b) return NULL;
    for(;;) {
        if(n==размер) { if(размер>SIZE_MAX/2) { free(b); return NULL; } размер*=2;
            unsigned char *новый=(unsigned char*)realloc(b,размер); if(!новый) { free(b); return NULL; } b=новый; }
        size_t взято=fread(b+n,1,размер-n,f); n+=взято;
        if(!взято) { if(ferror(f)) { free(b); return NULL; } break; }
    }
    *длина=n; return b;
}
static int самопроверка(void)
{
    static const unsigned char исходник[]="сказать речь Привет, (имя)! конец речи.\nпусть число равно 12.";
    static const unsigned char латинское[]="hello";
    static const unsigned char речь_без_конца[]="сказать речь не закрыта";
    size_t n=0;
    if(разобрать_поток(исходник,sizeof(исходник)-1,0,0,NULL,&n)!=0||n!=8) return 1;
    if(разобрать_поток(латинское,sizeof(латинское)-1,0,0,NULL,NULL)==0) return 2;
    if(разобрать_поток(речь_без_конца,sizeof(речь_без_конца)-1,0,0,NULL,NULL)==0) return 3;
    if(strcmp(найти_служебную_лемму("скажи"),"сказать")!=0) return 4;
    if(strcmp(найти_служебную_лемму("покажи"),"сказать")!=0) return 5;
    if(найти_служебную_лемму("привет")!=NULL) return 6;
    if(strcmp(найти_служебную_лемму("Скажи"),"сказать")!=0) return 7;
    МорфологическийВариант формы[32];
    size_t количество=морфология_разобрать("счётчику",формы,32);
    int найдено=0;
    for(size_t i=0;i<количество;++i)
        if(strcmp(формы[i].основа,"счётчик")==0 && strcmp(формы[i].падеж,"дт")==0 &&
           strcmp(формы[i].число,"ед")==0) найдено=1;
    if(!найдено) return 8;
    количество=морфология_разобрать("СУММУ",формы,32); найдено=0;
    for(size_t i=0;i<количество;++i)
        if(strcmp(формы[i].основа,"сумм")==0 && strcmp(формы[i].падеж,"дт")==0) найдено=1;
    if(!найдено) return 9;
    количество=морфология_разобрать("перечня",формы,32); найдено=0;
    for(size_t i=0;i<количество;++i)
        if(strcmp(формы[i].основа,"перечн")==0 && strcmp(формы[i].падеж,"рд")==0) найдено=1;
    if(!найдено) return 10;
    ИменнаяФорма именные[12];
    количество=склонить_имя_нативно("книга","ж",0,именные);
    if(количество!=12 || strcmp(именные[4].форма,"книгой")!=0 ||
       strcmp(именные[7].форма,"книг")!=0 || strcmp(именные[10].форма,"книгами")!=0)
        return 11;
    количество=склонить_имя_нативно("студент","м",1,именные);
    if(количество!=12 || strcmp(именные[3].форма,"студента")!=0 ||
       strcmp(именные[9].форма,"студентов")!=0) return 12;
    количество=склонить_имя_нативно("окно","с",0,именные);
    if(количество!=12 || strcmp(именные[6].форма,"окна")!=0 ||
       strcmp(именные[11].форма,"окнах")!=0) return 13;
    количество=склонить_имя_нативно("покупка","ж",0,именные);
    if(количество!=12 || strcmp(именные[7].форма,"покупок")!=0) return 14;
    количество=склонить_имя_нативно("ручка","ж",0,именные);
    if(количество!=12 || strcmp(именные[7].форма,"ручек")!=0) return 15;
    return 0;
}
int main(int argc,char **argv)
{
    /* Явный выбор нативного пути позволяет общему запускателю не вызывать Python. */
    if (argc >= 2 && strcmp(argv[1], "--нативно") == 0) { --argc; ++argv; }
    if (argc>=2 && (strcmp(argv[1],"выполнить")==0 || strcmp(argv[1],"запустить")==0)) {
        if (argc<3) { fputs("Использование: лексус-ядро выполнить файл.lek [--профиль=18+] [--профиль=зумер]\n",stderr); return 2; }
        unsigned профили=ЛЕКСУС_ПРОФИЛЬ_БЕЗ_ИЗМЕНЕНИЙ;
        for(int i=3;i<argc;++i) {
            if(strcmp(argv[i],"--профиль=18+")==0) профили|=ЛЕКСУС_ПРОФИЛЬ_18_ПЛЮС;
            else if(strcmp(argv[i],"--профиль=зумер")==0) профили|=ЛЕКСУС_ПРОФИЛЬ_ЗУМЕР;
            else { fprintf(stderr,"Лексус: неизвестный параметр профиля: %s\n",argv[i]); return 2; }
        }
        FILE *f=fopen(argv[2],"rb");
        if(!f){perror("Лексус: не удалось открыть исходник");return 2;}
        size_t length=0; unsigned char *source=прочитать(f,&length); fclose(f);
        if(!source){fputs("Лексус: не удалось прочитать исходник.\n",stderr);return 2;}
        Счётчик счётчик={0,0};
        if(!utf8_проверить(source,length,&счётчик)){
            free(source); fputs("Лексус: исходник содержит некорректный UTF-8.\n",stderr); return 2;
        }
        ЛексусСкомпилированнаяПрограмма compiled; char error[512]={0};
        if(!лексус_скомпилировать_с_профилями((const char*)source,length,профили,&compiled,error,sizeof(error))){
            fprintf(stderr,"Лексус: ошибка компиляции: %s.\n",error[0]?error:"неизвестная ошибка");
            free(source); return 1;
        }
        free(source);
        ЛексусПрограмма vm={compiled.код,compiled.число_инструкций,
                             compiled.константы,compiled.число_констант,compiled.регистры};
        ЛексусЗначение result=лексус_ничто(); error[0]=0;
        int ok=лексус_машина_исполнить(&vm,10000000u,&result,error,sizeof(error));
        if(!ok) fprintf(stderr,"Лексус: ошибка исполнения: %s.\n",error[0]?error:"неизвестная ошибка");
        лексус_значение_освободить(&result);
        лексус_скомпиляцию_освободить(&compiled);
        return ok?0:1;
    }
    if (argc >= 3 && strcmp(argv[1], "слово") == 0) {
        const char *path = argc >= 4 ? argv[3] : "ядро/русские_формы.lxd";
        ЛексусИндекс index; char error[256];
        if (!лексус_индекс_открыть(&index, path, error, sizeof(error))) {
            fprintf(stderr, "Лексус: %s\n", error); return 2;
        }
        size_t length = strlen(argv[2]);
        char *normalized = (char *)malloc(length + 1);
        if (!normalized || !русское_нижнее(argv[2], normalized, length + 1)) {
            free(normalized); лексус_индекс_закрыть(&index);
            fputs("Лексус: неверная UTF-8-форма слова.\n", stderr); return 2;
        }
        int found = лексус_индекс_найти(&index, normalized);
        free(normalized);
        puts(found ? "Слово есть в лексической базе." : "Слово не найдено в лексической базе.");
        лексус_индекс_закрыть(&index);
        return found ? 0 : 1;
    }
    if (argc >= 3 && strcmp(argv[1], "подсказки") == 0) {
        const char *path = argc >= 4 ? argv[3] : "ядро/русские_формы.lxd";
        ЛексусИндекс index; char error[256]; const char *items[20];
        if (!лексус_индекс_открыть(&index, path, error, sizeof(error))) {
            fprintf(stderr, "Лексус: %s\n", error); return 2;
        }
        size_t length = strlen(argv[2]);
        char *normalized = (char *)malloc(length + 1);
        if (!normalized || !русское_нижнее(argv[2], normalized, length + 1)) {
            free(normalized); лексус_индекс_закрыть(&index);
            fputs("Лексус: неверный UTF-8-префикс.\n", stderr); return 2;
        }
        size_t n = лексус_индекс_подсказать(&index, normalized, items, 20);
        for (size_t i = 0; i < n; ++i) puts(items[i]);
        free(normalized);
        лексус_индекс_закрыть(&index);
        return 0;
    }
    /* Основные команды — русские. Старые флаги оставлены для совместимости. */
    int версия = argc==2 && (strcmp(argv[1],"версия")==0 || strcmp(argv[1],"--version")==0);
    int проверка = argc==2 && (strcmp(argv[1],"самопроверка")==0 || strcmp(argv[1],"--self-test")==0);
    if(версия) { puts("Лексус " ЛЕКСУС_ВЕРСИЯ); return 0; }
    if(проверка) {
        int r=самопроверка();
        if(!r) puts("Нативное ядро: самопроверка пройдена.");
        else fprintf(stderr,"Нативное ядро: ошибка проверки %d.\n",r);
        return r;
    }
    if(argc>=3 && strcmp(argv[1],"склонять")==0) {
        const char *род = argc>=4 ? argv[3] : "м";
        int одуш = argc>=5 && strcmp(argv[4],"одушевлённое")==0;
        ИменнаяФорма формы[12];
        size_t число = склонить_имя_нативно(argv[2],род,одуш,формы);
        if(число!=12) { fputs("Лексус: не удалось склонить слово.\n",stderr); return 2; }
        for(size_t i=0;i<число;++i)
            printf("%s %s: %s\n",формы[i].падеж,формы[i].число,формы[i].форма);
        return 0;
    }
    int разбор = argc>=2 && (strcmp(argv[1],"разобрать")==0 || strcmp(argv[1],"--lex")==0);
    int кодировка = argc>=2 && (strcmp(argv[1],"проверить")==0 || strcmp(argv[1],"--check-utf8")==0);
    if(разбор||кодировка) {
        if(argc>3) { fputs("Лексус: укажите не более одного имени файла.\n",stderr); return 2; }
        FILE *f=stdin;
        if(argc==3 && strcmp(argv[2],"-")!=0) {
            f=fopen(argv[2],"rb");
            if(!f) { perror("Лексус: не удалось открыть файл"); return 2; }
        }
        size_t n=0; unsigned char *d=прочитать(f,&n); if(f!=stdin) fclose(f);
        if(!d) { fputs("Лексус: не удалось прочитать вход.\n",stderr); return 2; }
        int result;
        if(кодировка) {
            Счётчик счётчик={0,0}; result=utf8_проверить(d,n,&счётчик)?0:1;
            if(result) fputs("Лексус: вход не является корректным UTF-8.\n",stderr);
            else printf("Проверка UTF-8 пройдена: %zu знаков, %zu строк.\n",счётчик.знаки,счётчик.строки);
        } else {
            ЛексусИндекс индекс; int индекс_открыт=0; char ошибка_словаря[256];
            if(лексус_индекс_открыть(&индекс,"ядро/русские_формы.lxd",ошибка_словаря,sizeof(ошибка_словаря)))
                индекс_открыт=1;
            else
                fprintf(stderr,"Лексус: общий словарь недоступен: %s\n",ошибка_словаря);
            size_t токенов=0;
            result=разобрать_поток(d,n,1,1,индекс_открыт?&индекс:NULL,&токенов);
            if(!result) printf("Всего токенов: %zu\n",токенов);
            if(индекс_открыт) лексус_индекс_закрыть(&индекс);
        }
        free(d); return result;
    }
    puts("Лексус — нативное ядро " ЛЕКСУС_ВЕРСИЯ);
    puts("Команды:");
    puts("  версия");
    puts("  выполнить <файл.lek> [--профиль=18+] [--профиль=зумер]");
    puts("  самопроверка");
    puts("  проверить [файл.lek]");
    puts("  разобрать [файл.lek]");
    puts("  склонять слово [м|ж|с] [одушевлённое]");
    puts("  слово <форма> [путь-к-индексу]");
    puts("  подсказки <начало> [путь-к-индексу]");
    puts("Нативное выполнение пока поддерживает объявление числовых переменных, арифметику и вывод.");
    puts("Для остальных конструкций используйте основной совместимый исполнитель.");
    return argc>1?2:0;
}
