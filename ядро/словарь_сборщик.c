#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

#define МИНИМУМ_ФОРМ 1500000u
#define ЗАГОЛОВОК 36u
typedef struct { char **слова; size_t число, ёмкость; } Список;
static const uint16_t CP1251[128]={
0x0402,0x0403,0x201A,0x0453,0x201E,0x2026,0x2020,0x2021,0x20AC,0x2030,0x0409,0x2039,0x040A,0x040C,0x040B,0x040F,
0x0452,0x2018,0x2019,0x201C,0x201D,0x2022,0x2013,0x2014,0xFFFF,0x2122,0x0459,0x203A,0x045A,0x045C,0x045B,0x045F,
0x00A0,0x040E,0x045E,0x0408,0x00A4,0x0490,0x00A6,0x00A7,0x0401,0x00A9,0x0404,0x00AB,0x00AC,0x00AD,0x00AE,0x0407,
0x00B0,0x00B1,0x0406,0x0456,0x0491,0x00B5,0x00B6,0x00B7,0x0451,0x2116,0x0454,0x00BB,0x0458,0x0405,0x0455,0x0457,
0x0410,0x0411,0x0412,0x0413,0x0414,0x0415,0x0416,0x0417,0x0418,0x0419,0x041A,0x041B,0x041C,0x041D,0x041E,0x041F,
0x0420,0x0421,0x0422,0x0423,0x0424,0x0425,0x0426,0x0427,0x0428,0x0429,0x042A,0x042B,0x042C,0x042D,0x042E,0x042F,
0x0430,0x0431,0x0432,0x0433,0x0434,0x0435,0x0436,0x0437,0x0438,0x0439,0x043A,0x043B,0x043C,0x043D,0x043E,0x043F,
0x0440,0x0441,0x0442,0x0443,0x0444,0x0445,0x0446,0x0447,0x0448,0x0449,0x044A,0x044B,0x044C,0x044D,0x044E,0x044F};
static int кир(uint32_t c){return c>=0x0400&&c<=0x052F;}
static size_t в_utf8(uint32_t c,char *o){
 if(c<0x80){o[0]=(char)c;return 1;}
 if(c<0x800){o[0]=(char)(0xC0|(c>>6));o[1]=(char)(0x80|(c&63));return 2;}
 o[0]=(char)(0xE0|(c>>12));o[1]=(char)(0x80|((c>>6)&63));o[2]=(char)(0x80|(c&63));return 3;
}
static char *преобразовать(const unsigned char *s,size_t n){
 char *out=(char*)malloc(n*3+1);size_t k=0;int has_ru=0,prev=0;if(!out)return NULL;
 while(n&&(s[n-1]=='\r'||s[n-1]=='\n'||s[n-1]==' '||s[n-1]=='\t'))--n;
 size_t i=0;while(i<n&&(s[i]==' '||s[i]=='\t'))++i;
 for(;i<n;++i){uint32_t c=s[i];if(c>=128){c=CP1251[c-128];if(c==0xFFFF){free(out);return NULL;}}
  if(c>='A'&&c<='Z') c+=32;
  if(c>=0x0410&&c<=0x042F) c+=32;
  if(c==0x0401) c=0x0451;
  int letter=(c>='a'&&c<='z')||кир(c);
  if(letter){if(кир(c))has_ru=1;prev=1;}
  else if((c==45||c==39)&&prev&&i+1<n){prev=0;}
  else{free(out);return NULL;}
  k+=в_utf8(c,out+k);
 }
 if(!k||!has_ru){free(out);return NULL;}out[k]=0;return out;
}
static int cmp(const void*a,const void*b){return strcmp(*(char*const*)a,*(char*const*)b);}
static int add(Список*v,char*w){
 if(v->число==v->ёмкость){size_t cap=v->ёмкость?v->ёмкость*2:65536;char**p=(char**)realloc(v->слова,cap*sizeof(char*));if(!p)return 0;v->слова=p;v->ёмкость=cap;}
 v->слова[v->число++]=w;return 1;
}
static uint32_t crc_step(uint32_t c,unsigned char b){c^=b;for(unsigned i=0;i<8;++i)c=(c>>1)^(0xEDB88320u&(uint32_t)-(int32_t)(c&1u));return c;}
static void put32(unsigned char*p,uint32_t v){p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static void put64(unsigned char*p,uint64_t v){put32(p,(uint32_t)v);put32(p+4,(uint32_t)(v>>32));}
static int write_index(const char*path,Список*v,uint32_t*checksum,uint64_t*file_size){
 uint64_t bytes=0;for(size_t i=0;i<v->число;++i)bytes+=(uint64_t)strlen(v->слова[i])+1;
 if(bytes>UINT32_MAX){fprintf(stderr,"Словарь превышает 4 ГБ.\n");return 0;}
 uint32_t*off=(uint32_t*)malloc((v->число+1)*sizeof(uint32_t));if(!off)return 0;
 uint32_t pos=0;for(size_t i=0;i<v->число;++i){off[i]=pos;pos+=(uint32_t)strlen(v->слова[i])+1;}off[v->число]=pos;
 uint32_t crc=0xFFFFFFFFu;for(size_t i=0;i<=v->число;++i){unsigned char b[4];put32(b,off[i]);for(int j=0;j<4;++j)crc=crc_step(crc,b[j]);}
 for(size_t i=0;i<v->число;++i){const unsigned char*p=(const unsigned char*)v->слова[i];size_t n=strlen(v->слова[i])+1;for(size_t j=0;j<n;++j)crc=crc_step(crc,p[j]);}crc=~crc;
 if(checksum)*checksum=crc;
 if(file_size)*file_size=(uint64_t)ЗАГОЛОВОК+(uint64_t)(v->число+1)*4+bytes;
 FILE*f=fopen(path,"wb");if(!f){free(off);perror(path);return 0;}
 unsigned char h[ЗАГОЛОВОК]={0};memcpy(h,"LEKSLXD1",8);put32(h+8,1);put32(h+12,ЗАГОЛОВОК);put64(h+16,v->число);put64(h+24,bytes);put32(h+32,crc);
 int ok=fwrite(h,1,sizeof(h),f)==sizeof(h);
 for(size_t i=0;ok&&i<=v->число;++i){unsigned char b[4];put32(b,off[i]);ok=fwrite(b,1,4,f)==4;}
 for(size_t i=0;ok&&i<v->число;++i){size_t n=strlen(v->слова[i])+1;ok=fwrite(v->слова[i],1,n,f)==n;}
 if(fclose(f)!=0) ok=0;
 free(off);
 return ok;
}
static double now_seconds(void){
#ifdef _WIN32
 /* MinGW runtimes may omit C11 timespec_get; clock() keeps the builder portable. */
 return (double)clock()/(double)CLOCKS_PER_SEC;
#else
 struct timespec ts;
 if(timespec_get(&ts,TIME_UTC)!=TIME_UTC)return 0.0;
 return (double)ts.tv_sec+(double)ts.tv_nsec/1000000000.0;
#endif
}
int main(int argc,char**argv){
 double started=now_seconds();
 if(argc!=3){fprintf(stderr,"Использование: словарь-сборщик <russian.txt CP1251> <русские_формы.lxd>\n");return 2;}
 FILE*in=fopen(argv[1],"rb");if(!in){perror(argv[1]);return 2;}
 Список v={0};char line[4096];size_t raw=0,bad=0;
 while(fgets(line,sizeof(line),in)){++raw;size_t n=strlen(line);if(n==sizeof(line)-1&&line[n-1]!='\n'){fprintf(stderr,"Строка %zu слишком длинная.\n",raw);fclose(in);return 2;}
  char*w=преобразовать((const unsigned char*)line,n);if(!w){++bad;continue;}if(!add(&v,w)){free(w);fclose(in);return 2;}}
 if(ferror(in)){fclose(in);return 2;}fclose(in);qsort(v.слова,v.число,sizeof(char*),cmp);
 size_t unique=0;for(size_t i=0;i<v.число;++i){if(unique&&strcmp(v.слова[unique-1],v.слова[i])==0){free(v.слова[i]);continue;}v.слова[unique++]=v.слова[i];}v.число=unique;
 if(v.число<МИНИМУМ_ФОРМ){fprintf(stderr,"Проверка источника не пройдена: уникальных форм %zu, минимум %u; отброшено %zu из %zu.\n",v.число,МИНИМУМ_ФОРМ,bad,raw);return 3;}
 uint32_t checksum=0;uint64_t file_size=0;
 if(!write_index(argv[2],&v,&checksum,&file_size)){fprintf(stderr,"Не удалось записать индекс.\n");return 4;}
 double seconds=now_seconds()-started;
 printf("Словарь собран: %zu уникальных форм; строк %zu; отброшено %zu; размер %llu байт; CRC32 %08X; время %.3f с.\n",v.число,raw,bad,(unsigned long long)file_size,(unsigned)checksum,seconds);
 for(size_t i=0;i<v.число;++i) free(v.слова[i]);
 free(v.слова);
 return 0;
}
