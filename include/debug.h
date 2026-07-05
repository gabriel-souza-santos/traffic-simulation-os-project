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

#if DEBUG

/**
 * @def TRY(expr_)
 * @brief Executa uma expressão e aborta o programa em caso de falha.
 *
 * Assume que a expressão retorna 0 para sucesso e qualquer outro valor
 * para falhas. É ideal para envolver chamadas da API Pthreads
 * (ex: `TRY(pthread_mutex_lock(&lock));`). Se a função falhar, a macro
 * captura o erro, imprime o nome do arquivo, a linha e a expressão exata
 * no fluxo de erro padrão (`stderr`), e encerra a simulação.
 *
 * @param expr_ A expressão ou chamada de função a ser executada e avaliada.
 */
#define TRY(expr_)                                  \
    do {                                            \
        if ((expr_) != 0) {                         \
            fprintf(stderr,                         \
                "On file %s:%d\n"                   \
                "Error: fail to execute '%s'\n",    \
                __FILE__,                           \
                __LINE__,                           \
                #expr_                              \
            );                                      \
            exit(EXIT_FAILURE);                     \
        }                                           \
    } while (0)
/* TRY */

/**
 * @def CHECK_NULL(ptr_)
 * @brief Checa se um determinado ponteiro é nulo e aborta se for o caso.
 *
 * Utilizada para garantir a segurança na manipulação de memória. Deve ser
 * usada logo após alocações dinâmicas (ex: `malloc`, `calloc`) ou no início
 * de funções para validar argumentos obrigatórios. Caso o ponteiro seja NULL,
 * relata o arquivo e a linha do erro no `stderr` e aborta a execução.
 *
 * @param ptr_ O ponteiro que será testado contra NULL.
 */
#define CHECK_NULL(ptr_)                            \
    do {                                            \
        if ((ptr_) == NULL) {                       \
            fprintf(stderr,                         \
                "On file %s:%d\n"                   \
                "Error: '%s' cannot be NULL\n"      \
                "A NULL pointer was provided or "   \
                "memory allocation has failed\n",   \
                __FILE__,                           \
                __LINE__,                           \
                #ptr_                               \
            );                                      \
            exit(EXIT_FAILURE);                     \
        }                                           \
    } while (0)
/* CHECK_NULL */

#else //#if DEBUG

/*
 * Modo Release (Produção):
 * As macros são redefinidas para minimizar o overhead de processamento.
 * TRY apenas executa a expressão silenciosamente e CHECK_NULL é ignorada.
 */

#define TRY(expr_) (expr_)
#define CHECK_NULL(ptr_) (ptr_)

#endif //#if DEBUG #else

#endif //URBAN_TRAFFIC_DEBUG_H