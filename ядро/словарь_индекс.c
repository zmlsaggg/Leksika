#include "словарь_индекс.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define ЗАГОЛОВОК 36u
#define МАКС_ФОРМ 5000000u
#define МАКС_ДАННЫХ (128ull * 1024ull * 1024ull)
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static uint64_t u64(const unsigned char *p){return (uint64_t)u32(p)|((uint64_t)u32(p+4)<<32);}
static uint32_t crc32(const unsigned char *p,size_t n){uint32_t c=0xFFFFFFFFu;for(size_t i=0;i<n;++i){c^=p[i];for(unsigned b=0;b<8;++b)c=(c>>1)^(0xEDB88320u&(uint32_t)-(int32_t)(c&1u));}return ~c;}
static int utf8_ok(const unsigned char *s,size_t n){
 size_t i=0;while(i<n){unsigned char a=s[i++];if(a<0x80)continue;unsigned k;uint32_t c;
 if(a>=0xC2&&a<=0xDF){k=1;c=a&31;}else if(a>=0xE0&&a<=0xEF){k=2;c=a&15;}else if(a>=0xF0&&a<=0xF4){k=3;c=a&7;}else return 0;
 if(i+k>n)return 0;for(unsigned j=0;j<k;++j){unsigned char b=s[i++];if((b&0xC0)!=0x80)return 0;
 if(j==0&&((a==0xE0&&b<0xA0)||(a==0xED&&b>=0xA0)||(a==0xF0&&b<0x90)||(a==0xF4&&b>=0x90)))return 0;c=(c<<6)|(b&63);}
 if(c>0x10FFFF||(c>=0xD800&&c<=0xDFFF))return 0;}return 1;}
static void seterr(char *p,size_t n,const char *s){if(p&&n)snprintf(p,n,"%s",s);}
static uint32_t off(const ЛексусИндекс *x,uint64_t n){return u32(x->смещения+n*4);}
int лексус_индекс_открыть(ЛексусИндекс *x,const char *path,char *err,size_t errn){
 if(!x||!path){seterr(err,errn,"не задан индекс или путь");return 0;}memset(x,0,sizeof(*x));
 FILE *f=fopen(path,"rb");if(!f){seterr(err,errn,"не удалось открыть встроенный словарь");return 0;}
 if(fseek(f,0,SEEK_END)!=0){fclose(f);seterr(err,errn,"ошибка размера словаря");return 0;}
 long len=ftell(f);if(len<(long)ЗАГОЛОВОК||(uint64_t)len>ЗАГОЛОВОК+((uint64_t)МАКС_ФОРМ+1)*4+МАКС_ДАННЫХ||fseek(f,0,SEEK_SET)!=0){fclose(f);seterr(err,errn,"повреждён заголовок словаря");return 0;}
 unsigned char *p=(unsigned char*)malloc((size_t)len);if(!p){fclose(f);seterr(err,errn,"не хватает памяти для словаря");return 0;}
 if(fread(p,1,(size_t)len,f)!=(size_t)len){free(p);fclose(f);seterr(err,errn,"не удалось прочитать словарь");return 0;}fclose(f);
 if(memcmp(p,"LEKSLXD1",8)||u32(p+8)!=1||u32(p+12)!=ЗАГОЛОВОК){free(p);seterr(err,errn,"неподдерживаемый формат словаря");return 0;}
 uint64_t count=u64(p+16),bytes=u64(p+24);if(!count||count>МАКС_ФОРМ||bytes>МАКС_ДАННЫХ){free(p);seterr(err,errn,"недопустимый размер словаря");return 0;}
 uint64_t table=(count+1)*4,total=ЗАГОЛОВОК+table+bytes;
 if(total!=(uint64_t)len||crc32(p+ЗАГОЛОВОК,(size_t)(table+bytes))!=u32(p+32)){free(p);seterr(err,errn,"размер или контрольная сумма словаря не совпадает");return 0;}
 x->файл=p;x->смещения=p+ЗАГОЛОВОК;x->данные=p+ЗАГОЛОВОК+table;x->количество=count;x->размер_данных=bytes;
 if(off(x,0)!=0||off(x,count)!=bytes){лексус_индекс_закрыть(x);seterr(err,errn,"неверные границы таблицы словаря");return 0;}
 for(uint64_t i=0;i<count;++i){uint32_t a=off(x,i),b=off(x,i+1);
  if(b<=a+1||b>bytes||x->данные[b-1]!=0||memchr(x->данные+a,0,b-a-1)||!utf8_ok(x->данные+a,b-a-1)){
   лексус_индекс_закрыть(x);seterr(err,errn,"повреждена запись словаря");return 0;}
  if(i&&strcmp((const char*)x->данные+off(x,i-1),(const char*)x->данные+a)>=0){
   лексус_индекс_закрыть(x);seterr(err,errn,"словарь не отсортирован или содержит дубликаты");return 0;}
 }return 1;
}
void лексус_индекс_закрыть(ЛексусИндекс *x){if(x){free(x->файл);memset(x,0,sizeof(*x));}}
int лексус_индекс_найти(const ЛексусИндекс *x,const char *word){
 if(!x||!x->файл||!word)return 0;uint64_t lo=0,hi=x->количество;
 while(lo<hi){uint64_t mid=lo+(hi-lo)/2;const char *w=(const char*)x->данные+off(x,mid);int c=strcmp(w,word);if(c<0)lo=mid+1;else hi=mid;}
 return lo<x->количество&&strcmp((const char*)x->данные+off(x,lo),word)==0;
}
size_t лексус_индекс_подсказать(const ЛексусИндекс *x,const char *prefix,const char **out,size_t cap){
 if(!x||!x->файл||!prefix||!out||!cap)return 0;size_t n=strlen(prefix);uint64_t lo=0,hi=x->количество;
 while(lo<hi){uint64_t mid=lo+(hi-lo)/2;const char *w=(const char*)x->данные+off(x,mid);if(strncmp(w,prefix,n)<0)lo=mid+1;else hi=mid;}
 size_t found=0;while(lo<x->количество&&found<cap){const char *w=(const char*)x->данные+off(x,lo++);if(strncmp(w,prefix,n))break;out[found++]=w;}return found;
}
