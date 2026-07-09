/**
 * @file debug.c
 *
 * @author Gabriel Souza
 * @date 2026-06-30
 */
#include <stddef.h>
#include <stdio.h>
#include <stdarg.h>
#include "debug.h"



static FILE *log_target = NULL;

/**
 * @brief Abre o arquivo de log dedicado. Chame uma vez no início do main,
 * antes de render_init_terminal() (que entra no alt-screen buffer).
 * Se falhar, debug_log() cai de volta para stderr automaticamente.
 */
void debug_log_init(const char *file_name) {
    log_target = fopen(file_name, "w");
    if (!log_target) {
        fprintf(stderr, "Warning: failed to open log file '%s', falling back to stderr\n", file_name);
    }
}

/**
 * @brief Fecha o arquivo de log, se aberto. Chame no fim do main,
 * de preferência depois de render_reset_terminal().
 */
void debug_log_close(void) {
    if (log_target) {
        fclose(log_target);
        log_target = NULL;
    }
}

void debug_log(
    const char *file,
    const int  line,
    const char *func,
    const char *fmt,
    ...)
{
    FILE *out = log_target ? log_target : stderr;

    fprintf(out, "At %s:%d (%s):\n", file, line, func);

    va_list args;
    va_start(args, fmt);
    vfprintf(out, fmt, args);
    va_end(args);

    fputc('\n', out);
    fflush(out);
}
