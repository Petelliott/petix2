// fully C99 compatible string.h
#ifndef LIBC_STRING_H
#define LIBC_STRING_H

// make sure size_t and NULL are defined
#include <stddef.h>

void *memcpy(void * restrict s1,
             const void * restrict s2,
             size_t n);

void *memmove(void *s1, const void *s2, size_t n);

char *strcpy(char * restrict s1,
             const char * restrict s2);

char *strncpy(char * restrict s1,
              const char * restrict s2,
              size_t n);

char *strcat(char * restrict s1,
             const char * restrict s2);

char *strncat(char * restrict s1,
              const char * restrict s2,
              size_t n);

int memcmp(const void *s1, const void *s2, size_t n);

int strcmp(const char *s1, const char *s2);

int strcoll(const char *s1, const char *s2);

int strncmp(const char *s1, const char *s2, size_t n);

size_t strxfrm(char * restrict s1,
               const char * restrict s2,
               size_t n);

void *memchr(const void *s, int c, size_t n);

char *strchr(const char *s, int c);

size_t strcspn(const char *s1, const char *s2);

char *strpbreak(const char *s1, const char *s2);

char *strrchr(const char *s, int c);

size_t strspn(const char *s1, const char *s2);

char *strstr(const char *s1, const char *s2);

char *strtok(char * restrict s1,
             const char * restrict s2);

char *strtok_r(char * restrict str,
               const char * restrict delim,
               char * restrict *save);

void *memset(void *s, int c, size_t n);

const char *strerror(int errnum);

size_t strlen(const char *s);

size_t strnlen(const char *s, size_t maxlen);

#endif
