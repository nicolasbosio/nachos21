///
///
///

#include "../userprog/syscall.h" // Include fix IntelliSense
#include "lib.h"

unsigned strlen(const char *s)
{
    if(s == NULL) return 0;
    unsigned n;
    for(n = 0; s[n] != '\0'; n++);
    return n + 1;
}

void strput(const char *s)
{
    int len = strlen(s);
    char z[len + 1];
    int i = 0;
    for (; i < len; i++) z[i] = s[i];
    z[i++] = '\n';
    Write(z, len + 1, CONSOLE_OUTPUT);
}

void swap(char *x, char *y) {
    char t = *x; *x = *y; *y = t;
}
 
void strrev(char *buffer, int i, int j)
{
    while (i < j) {
        swap(&buffer[i++], &buffer[j--]);
    }
}
 
void itoa(int value, char* buffer)
{ 
    int sign;
    if ((sign=value) < 0) value = -value;
    int i = 0;

    while (value)
    {
        int r = value % 10;
        buffer[i++] = 48 + r;
        value = value / 10;
    }

    if (i == 0) 
        buffer[i++] = '0';

    if (sign < 0)
        buffer[i++] = '-';
 
    buffer[i] = '\0';
 
    strrev(buffer, 0, i - 1);
}

int strcmp(char* a, char* b)
{
    int lenA = strlen(a);
    int lenB = strlen(b);
    if(lenA != lenB) return 0;
    int i = 0;
    for (; i < lenA; i++)
    {
        if(a[i] != b[i]) return 0;
    }
    return 1;
}