#ifndef clox_stdio_h
#define clox_stdio_h

#include <stdio.h>

#define printf Serial_printf
#define fprintf Serial_fprintf
#define vfprintf Serial_vfprintf
#define fputs(output, stream) Serial_fprintf(stream, "%s", output)

int Serial_printf(const char *fmt, ...);
int Serial_fprintf(FILE *dummy, const char *fmt, ...);

#endif
