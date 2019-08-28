#include<stdio.h>
#include"watchpoint.h"

volatile int a = 0;
int b=11;


WP_TriggerAction_t OnTrap(WP_TriggerInfo_t *info){

    printf("One trap, secret %d, pc %lx, %s\n", *((int *)info->data), info->pc, info->pcPrecise==1?"Precise":"Imprecise");
    return WP_DISABLE;
}


int main(int argc, char **argv){

    WP_Init();

    WP_ThreadInit(OnTrap);
    printf("The address of a is %p\n", &a);
    WP_Subscribe(&a, 1, WP_RW, &b);
    a += 1;

    WP_ThreadTerminate(0);

    WP_Shutdown();
    return 0;
}
