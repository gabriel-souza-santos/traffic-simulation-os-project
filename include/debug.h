/**
 * @file debug.h
 * @brief Macros utilitárias para tratamento de erros e depuração.
 *
 * Este arquivo fornece ferramentas simplificadas para a verificação de erros
 * durante o desenvolvimento do simulador. É especialmente útil para validar
 * alocações de memória e checar automaticamente o retorno de funções da
 * biblioteca Pthreads, garantindo que o programa aborte imediatamente em
 * caso de estados inconsistentes.
 *
 * @date 2026-06-25
 */

#ifndef URBAN_TRAFFIC_DEBUG_H
#define URBAN_TRAFFIC_DEBUG_H

#include <stdio.h>
#include <stdlib.h>

/**
 * @brief Flag de compilação para ativar/desativar o modo de depuração.
 * * Se não for definida externamente (ex: via flag -DDEBUG=1 no Makefile),
 * o modo de depuração será ativado por padrão.
 */
#ifndef DEBUG
#define DEBUG 1
#endif //DEBUG

/**
 * @brief Registra uma mensagem de depuração formatada no fluxo de erro padrão.
 *
 * Função base utilizada internamente pelas macros deste módulo (TRY, LOG,
 * LOG_IF, CHECK_NULL). Em geral não deve ser chamada diretamente — prefira
 * as macros, que preenchem automaticamente os parâmetros de contexto
 * (__FILE__, __LINE__, __func__).
 *
 * @param file Nome do arquivo-fonte onde a chamada ocorreu.
 * @param line Número da linha onde a chamada ocorreu.
 * @param func Nome da função onde a chamada ocorreu.
 * @param fmt  String de formato (estilo printf) da mensagem a ser exibida.
 * @param ...  Argumentos variádicos correspondentes ao formato.
 */
void debug_log(
    const char *file,
    const int  line,
    const char *func,
    const char *fmt,
    ...);

void debug_log_init(const char *file_name);
void debug_log_close(void);

#if DEBUG

#define DEBUG_INIT(file) \
    (debug_log_init(file))

#define DEBUG_CLOSE \
    (debug_log_close())

/**
 * @def TRY(expr)
 * @brief Executa uma expressão e aborta o programa em caso de falha.
 *
 * Assume que a expressão retorna 0 para sucesso e qualquer outro valor
 * para falhas. É ideal para envolver chamadas da API Pthreads
 * (ex: `TRY(pthread_mutex_lock(&lock));`). Se a função falhar, a macro
 * captura o erro, imprime o nome do arquivo, a linha e a expressão exata
 * no fluxo de erro padrão (`stderr`), e encerra a simulação.
 *
 * @param expr A expressão ou chamada de função a ser executada e avaliada.
 */
#define TRY(expr)                                   \
    do {                                            \
        if ((expr) != 0) {                          \
            debug_log(                              \
                __FILE__, __LINE__, __func__,       \
                "Error: fail to execute '%s'",      \
                #expr);                             \
            exit(EXIT_FAILURE);                     \
        }                                           \
    } while (0)


/**
 * @def LOG(...)
 * @brief Registra uma mensagem de depuração com contexto automático de arquivo,
 *        linha e função.
 *
 * Atalho para debug_log que injeta automaticamente __FILE__, __LINE__ e
 * __func__. Aceita os mesmos argumentos de formato que printf.
 *
 * Compilado como no-op em modo Release (DEBUG=0).
 *
 * @param ... String de formato (estilo printf) seguida dos argumentos
 *            correspondentes.
 */
#define LOG(...) \
    debug_log(__FILE__, __LINE__, __func__, __VA_ARGS__)


/**
 * @def LOG_IF(cond, ...)
 * @brief Registra uma mensagem de depuração condicionalmente.
 *
 * Equivalente a LOG, mas a mensagem só é emitida se @p cond for verdadeiro.
 * A condição é avaliada apenas uma vez, sem efeitos colaterais de avaliação
 * múltipla.
 *
 * Compilado como no-op em modo Release (DEBUG=0).
 *
 * @param cond Condição booleana que habilita o log quando verdadeira.
 * @param ...  String de formato (estilo printf) seguida dos argumentos
 *             correspondentes.
 */
#define LOG_IF(cond, ...)                           \
    do {                                            \
        if (cond) {                                 \
            LOG(__VA_ARGS__);                       \
        }                                           \
    } while (0)


/**
 * @def CHECK_NULL(ptr)
 * @brief Checa se um determinado ponteiro é nulo e aborta se for o caso.
 *
 * @deprecated Prefira usar @c LOG ou @c LOG_IF.
 *
 * Utilizada para garantir a segurança na manipulação de memória. Deve ser
 * usada logo após alocações dinâmicas (ex: `malloc`, `calloc`) ou no início
 * de funções para validar argumentos obrigatórios. Caso o ponteiro seja NULL,
 * relata o arquivo e a linha do erro no `stderr` e aborta a execução.
 *
 * @param ptr O ponteiro que será testado contra NULL.
 */
#define CHECK_NULL(ptr)                             \
    do {                                            \
        if ((ptr) == NULL) {                        \
            fprintf(stderr,                         \
                "At %s:%d (%s):\n"                  \
                "Error: '%s' cannot be NULL\n"      \
                "A NULL pointer was provided or "   \
                "memory allocation has failed\n",   \
                __FILE__,                           \
                __LINE__,                           \
                __func__,                           \
                #ptr                                \
            );                                      \
            exit(EXIT_FAILURE);                     \
        }                                           \
    } while (0)


#else //#if DEBUG

/*
 * Modo Release (Produção):
 * As macros são redefinidas para minimizar o overhead de processamento.
 */


#define DEBUG_INIT(file)    do { } while (0)
#define DEBUG_CLOSE         do { } while (0)
#define TRY(expr)           if (expr) exit(EXIT_FAILURE);
#define LOG(...)            do { } while (0)
#define LOG_IF(cond, ...)   do { } while (0)
#define CHECK_NULL(ptr)     do { } while (0)

#endif //#if DEBUG #else

#endif //URBAN_TRAFFIC_DEBUG_H