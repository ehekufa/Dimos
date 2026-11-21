#ifndef DIMAOS_RTL_H
#define DIMAOS_RTL_H

#include "types.h"

void* memcpy(void* dest, const void* src, size_t n);
void* memset(void* dest, int c, size_t n);
void* memmove(void* dest, const void* src, size_t n);
int   memcmp(const void* s1, const void* s2, size_t n);

size_t strlen(const char* s);
int    strcmp(const char* s1, const char* s2);
int    strncmp(const char* s1, const char* s2, size_t n);
int    strcasecmp(const char* s1, const char* s2);
int    strncasecmp(const char* s1, const char* s2, size_t n);
char*  strcpy(char* dest, const char* src);
char*  strncpy(char* dest, const char* src, size_t n);
char*  strcat(char* dest, const char* src);
char*  strchr(const char* s, int c);
int    toupper(int c);

int  vsnprintf(char* buf, size_t size, const char* fmt, va_list args);
int  snprintf(char* buf, size_t size, const char* fmt, ...);
int  atoi(const char* s);
int  abs(int x);
long labs(long x);

#endif /* DIMAOS_RTL_H */
