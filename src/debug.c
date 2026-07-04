/**
 * @file debug.c
 *
 * @author Gabriel Souza
 * @date 2026-06-30
 */

#include <stdio.h>
#include <stdarg.h>
#include "debug.h"

void debug_log(
    const char *file,
    const int  line,
    const char *func,
    const char *fmt,
    ...)
{
    fprintf(stderr, "At %s:%d (%s):\n", file, line, func);

    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);

    fputc('\n', stderr);
}
