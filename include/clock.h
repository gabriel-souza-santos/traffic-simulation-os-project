/**
 * @file clock.h
 * @brief Gerenciamento do relógio para a simulação de tráfego.
 *
 * Este módulo define a estrutura e as operações do relógio que coordena
 * a passagem do tempo na simulação em "ticks". Ele é responsável por
 * garantir a sincronização entre a thread do tempo e as threads dos veículos,
 * evitando espera ocupada (busy waiting) através de mecanismos de concorrência.
 *
 * @date 2026-06-11
 */
#ifndef URBAN_TRAFFIC_CLOCK_H
#define URBAN_TRAFFIC_CLOCK_H

#include <stddef.h>

/**
 * @brief Número total de ticks da simulação.
 *
 * @note Poderá ser removido posteriormente e substituído
 * por booleanos/variáveis de condição internas para indicar que
 * a simulação está ativa.
 */
#define TICKS 20


/**
 * @brief Tipo opaco que representa o relógio global e seus mecanismos de sincronização.
 *
 * Deve ser instanciado sempre por meio de um ponteiro:
 * @code
 * Clock *clock;
 * @endcode
 */
typedef struct Clock Clock;


/**
 * @brief Cria e inicializa uma nova instância do relógio.
 *
 * Aloca dinamicamente a estrutura do relógio e inicializa seus recursos
 * internos de sincronização (mutexes e variáveis de condição), definindo
 * o tick inicial como zero.
 *
 * @return Um ponteiro para a estrutura recém-criada.
 */
Clock *clock_new(void);


/**
 * @brief Limpa os recursos alocados para o relógio.
 * * Destrói os mutexes e variáveis de condição internos de forma segura
 * e libera a memória alocada para a estrutura do relógio. Deve ser
 * chamada ao final da simulação para evitar vazamento de memória.
 * * @param clock Ponteiro para o relógio que será destruído.
 */
void clock_destroy(Clock *clock);


/**
 * @brief Rotina principal da thread do relógio responsável por avançar o tempo.
 *
 * Esta função deve ser passada para `pthread_create`. Ela executa um loop
 * onde ela espera as outras threads finalizarem, incrementa o tick global
 * de forma e emite um sinal (broadcast) para acordar todas as threads que estão
 * bloqueadas aguardando o próximo tick.
 *
 * @param clock Ponteiro para o relógio do sistema. Usa void* para seguir a
 * assinatura de `pthread_create`.
 *
 * @return NULL, para respeitar a assinatura padrão exigida pela API Pthreads.
 */
void *clock_update(void *clock);


/**
 * @brief Sinaliza ao relógio a conclusão do trabalho no tick atual e aguarda o próximo.
 *
 * Chamada pelas threads dos veículos/semáforos. Esta função bloqueia a thread
 * chamadora (sem consumir CPU) até que o `clock_update` avance o tempo para um
 * tick maior que o tick informado.
 *
 * * Exemplo de uso:
 * @code{.c}
 * Clock *clock = clock_new();
 *
 * // ... dentro da thread do veículo ...
 * clock_signal(clock, clock_get_tick(clock)); // Dorme até o tick mudar
 * @endcode
 *
 * @param clock Ponteiro para o relógio do sistema.
 * @param tick O valor do tick no qual a thread acabou de finalizar o seu processamento.
 */
void clock_signal(Clock *clock, size_t tick);


/**
 * @brief Retorna o valor do tick atual da simulação.
 *
 * @param clock Ponteiro para o relógio do sistema
 *
 * @return O número exato do tick atual da simulação.
 */
size_t clock_get_tick(Clock *clock);

#endif //URBAN_TRAFFIC_CLOCK_H