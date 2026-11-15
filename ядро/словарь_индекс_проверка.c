#include "словарь_индекс.h"
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
static void p32(unsigned char*p,uint32_t v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static void p64(unsigned char*p,uint64_t v){p32(p,(uint32_t)v);p32(p+4,(uint32_t)(v>>32));}
static uint32_t step(uint32_t c,unsigned char b){c^=b;for(int i=0;i<8;++i)c=(c>>1)^(0xEDB88320u&(uint32_t)-(int32_t)(c&1u));return c;}
static int fixture(const char*path){
 const char*w[]={"абажур","абак","автор"};uint32_t off[4];uint32_t bytes=0;
 for(int i=0;i<3;++i){off[i]=bytes;bytes+=(uint32_t)strlen(w[i])+1;}off[3]=bytes;
 unsigned char *payload=(unsigned char*)malloc(16+bytes);if(!payload)return 0;
 for(int i=0;i<4;++i)p32(payload+i*4,off[i]);
 size_t at=16;for(int i=0;i<3;++i){size_t n=strlen(w[i])+1;memcpy(payload+at,w[i],n);at+=n;}
 uint32_t crc=0xFFFFFFFFu;for(size_t i=0;i<at;++i)crc=step(crc,payload[i]);crc=~crc;
 unsigned char h[36]={0};memcpy(h,"LEKSLXD1",8);p32(h+8,1);p32(h+12,36);p64(h+16,3);p64(h+24,bytes);p32(h+32,crc);
 FILE*f=fopen(path,"wb");if(!f){free(payload);return 0;}
 int ok=fwrite(h,1,36,f)==36&&fwrite(payload,1,at,f)==at;fclose(f);free(payload);return ok;
}
int main(void){
 const char*path="leksus_dictionary_test.lxd";if(!fixture(path)){puts("fixture creation failed");return 1;}
 ЛексусИндекс x;char err[256];if(!лексус_индекс_открыть(&x,path,err,sizeof(err))){printf("open failed: %s\n",err);remove(path);return 2;}
 if(!лексус_индекс_найти(&x,"абажур")||!лексус_индекс_найти(&x,"автор")||лексус_индекс_найти(&x,"несуществующее")||лексус_индекс_найти(&x,"АБАЖУР")){puts("exact lookup failed");лексус_индекс_закрыть(&x);remove(path);return 3;}
 const char*items[4];size_t n=лексус_индекс_подсказать(&x,"аба",items,4);
 if(n!=2||strcmp(items[0],"абажур")||strcmp(items[1],"абак")){puts("prefix lookup failed");лексус_индекс_закрыть(&x);remove(path);return 4;}
 лексус_индекс_закрыть(&x);
 FILE*f=fopen(path,"r+b");if(!f){remove(path);return 5;}fseek(f,-1,SEEK_END);int c=fgetc(f);fseek(f,-1,SEEK_END);fputc(c^1,f);fclose(f);
 if(лексус_индекс_открыть(&x,path,err,sizeof(err))){puts("corrupted index accepted");лексус_индекс_закрыть(&x);remove(path);return 6;}
 remove(path);puts("Бинарный словарь: поиск, подсказки и контроль целостности пройдены.");return 0;
}
