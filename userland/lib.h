///
///
///

#include "syscall.h"

#define NULL  ((void *) 0)
#define EOF '\0'
#define MAX_LINE 120
#define ERROR_RET_VALUE ((int)-1)

unsigned strlen(const char *s);
void strput(const char *s);
void swap(char *x, char *y);
void reverse(char *buffer, int i, int j);
void itoa(int n, char *str);
int strcmp(char* a, char* b);