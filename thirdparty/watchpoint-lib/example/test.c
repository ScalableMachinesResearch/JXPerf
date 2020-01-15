#include<stdio.h>
#include"watchpoint.h"

volatile int a = 1;

WP_TriggerAction_t OnTrap(WP_TriggerInfo_t *info){

    printf("One trap, value %d, address %p\n", *((int *)info->va), info->va);
    return WP_DISABLE;
}


int main(int argc, char **argv){

    WP_Init();

    WP_ThreadInit(OnTrap);
    printf("The address of a is %p\n", &a);
    WP_Subscribe((void *)(&a), sizeof(int), WP_RW, sizeof(int), NULL, 0, true);
    a += 1;

    WP_ThreadTerminate(0);

    WP_Shutdown();
    return 0;
}
