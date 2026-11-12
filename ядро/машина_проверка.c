#include "машина.h"
#include <stdio.h>
#include <string.h>
static int check(const char*n,int ok){printf("%s: %s\n",ok?"OK":"FAIL",n);return ok?0:1;}
int main(void){
 int failed=0;char err[256]={0};ЛексусЗначение constants[3]={лексус_ничто(),лексус_ничто(),лексус_ничто()};
 (void)лексус_число_создать("2",&constants[0]);(void)лексус_число_создать("3",&constants[1]);(void)лексус_число_создать("5",&constants[2]);
 const ЛексусИнструкция sumcode[]={
  {ЛЕКСУС_ОП_КОНСТАНТА,0,0,0,0},{ЛЕКСУС_ОП_КОНСТАНТА,1,0,0,1},
  {ЛЕКСУС_ОП_СЛОЖИТЬ,2,0,1,0},{ЛЕКСУС_ОП_ВОЗВРАТ,0,2,0,0}};
 ЛексусПрограмма sum={sumcode,4,constants,3,4};ЛексусЗначение result=лексус_ничто();
 int ok=лексус_машина_исполнить(&sum,100,&result,err,sizeof(err));int equal=0;
 if(ok)(void)лексус_равно(result,constants[2],&equal);
 failed+=check("регистровая арифметика 2 + 3 = 5",ok&&equal);лексус_значение_освободить(&result);
 ЛексусЗначение branch_constants[3]={лексус_логическое(0),лексус_ничто(),лексус_ничто()};
 (void)лексус_число_создать("99",&branch_constants[1]);(void)лексус_число_создать("7",&branch_constants[2]);
 const ЛексусИнструкция branchcode[]={
  {ЛЕКСУС_ОП_КОНСТАНТА,0,0,0,0},{ЛЕКСУС_ОП_ПЕРЕХОД_ЕСЛИ_ЛОЖЬ,0,0,0,3},
  {ЛЕКСУС_ОП_КОНСТАНТА,1,0,0,1},{ЛЕКСУС_ОП_КОНСТАНТА,1,0,0,2},
  {ЛЕКСУС_ОП_ВОЗВРАТ,0,1,0,0}};
 ЛексусПрограмма branch={branchcode,5,branch_constants,3,2};result=лексус_ничто();err[0]=0;
 ok=лексус_машина_исполнить(&branch,100,&result,err,sizeof(err));equal=0;if(ok)(void)лексус_равно(result,branch_constants[2],&equal);
 failed+=check("условный переход по ложному значению",ok&&equal);лексус_значение_освободить(&result);
 const ЛексусИнструкция loopcode[]={{ЛЕКСУС_ОП_ПЕРЕХОД,0,0,0,0}};
 ЛексусПрограмма loop={loopcode,1,NULL,0,1};err[0]=0;
 ok=лексус_машина_исполнить(&loop,5,&result,err,sizeof(err));
 failed+=check("ограничение бесконечного цикла",!ok&&strstr(err,"предел")!=NULL);
 const ЛексусИнструкция badcode[]={{ЛЕКСУС_ОП_КОПИЯ,9,0,0,0}};
 ЛексусПрограмма bad={badcode,1,NULL,0,2};err[0]=0;
 failed+=check("верификатор отклоняет неверный регистр",!лексус_байткод_проверить(&bad,err,sizeof(err)));
 for(int i=0;i<3;++i)лексус_значение_освободить(&constants[i]);
 for(int i=0;i<3;++i)лексус_значение_освободить(&branch_constants[i]);
 printf("Итого: %s\n",failed?"есть ошибки":"все проверки пройдены");return failed?1:0;
}
