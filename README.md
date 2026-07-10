# Simulador de Tráfego Urbano em C
 
O Simulador de Tráfego Urbano é um projeto de sistema desenvolvido em C utilizando threads, mutexes, semáforos e variáveis de condição
para demonstrar conceitos de Sistemas Operacionais como sincronização, exclusão mútua, coordenação de threads
e prevenção de deadlocks.
 
---
 
## Como executar
 
### 1. Instalar o WSL (apenas Windows)
```bash
wsl --install
```
 
### 2. Compilar e executar
No terminal (Linux ou WSL), execute:

```bash
# 1. Gere os arquivos de configuração do CMake na pasta build
cmake -S . -B build

# 2. Compile o projeto
cmake --build build

# 3. Execute o simulador
./build/bin/urban_trafic
```

Comando Único
```bash
cmake -S . -B build && cmake --build build && ./build/bin/urban_traffic
```
 
> Devido ao uso da biblioteca POSIX Threads, a compatibilidade deste projeto é limitada a sistemas Linux ou ambientes compatíveis (WSL no Windows).
 
---

## Estrutura do projeto

```simulador de trafego
.
├── 📁 include
│   ├── 📄 clock.h         # Definições do relógio lógico e barreiras de sincronização
│   ├── 📄 deadlock.h      # Protótipos para monitoramento e prevenção de deadlocks
│   ├── 📄 debug.h         # Macros de logging e ferramentas de depuração do sistema
│   ├── 📄 map.h           # Estrutura da malha urbana, células e tipos de tiles
│   ├── 📄 render.h        # Funções de exibição e renderização da simulação em ASCII
│   ├── 📄 traffic_light.h # Lógica e estruturas de controle dos semáforos
│   └── 📄 vehicle.h       # Modelagem, propriedades e ciclo de vida dos veículos
│
├── 📁 out
│    └── 📄 debug.log               # Arquivo de dubug em caso de erros
│
├── 📁 res
│   └── 📁 data                     # Diretório para caregar dados no programa
│    │   └── 📄 map.txt             # Representação do mapa a ser carregado
│    ├── 📁 tile                    # Diretório da reepresentação visual dos elementos da rua
│    │   ├── 📄 tile-blocked.txt    # Representação visual da casa
│    │   └── 📄 tile-road.txt       # Representação visual da calçada
│    ├── 📁 traffic-light           # Diretório da reepresentação visual dos semáforos
│    │   ├── 📄 light-green.txt     # Representação visual do semaforo verde
│    │   ├── 📄 light-red.txt       # Representação visual do semaforo vermelho
│    │   └── 📄 light-yellow.txt    # Representação visual do semaforo amarelo
│    │
│    └── 📁 vehicle                      # Diretório da reepresentação visual dos veículos
│         ├── 📄 ambulance-right.txt     # Representação visual da ambulância (prioridade)
│         ├── 📄 ambulance-left.txt      # Representação visual da ambulância (prioridade)
│         ├── 📄 car-fast-right.txt      # Representação visual do carro na direita (alta velocidade)
│         ├── 📄 car-fast-left.txt       # Representação visual do carro na esquerda (alta velocidade)
│         ├── 📄 car-medium-right.txt    # Representação visual do carro na direita (média velocidade)
│         ├── 📄 car-medium-left.txt     # Representação visual do carro na esquerda (média velocidade)
│         ├── 📄 car-slow-right.txt      # Representação visual do carro na direita (baixa velocidade)
│         └── 📄 car-slow-left.txt       # Representação visual do carro na esquerda (baixa velocidade)
│
└── 📁 src
    ├── 📄 clock.c         # Implementação da sincronização baseada em turnos 
    ├── 📄 deadlock.c      # Algoritmos de tratamento de impasses entre threads
    ├── 📄 debug.c         # Gerenciamento de saída de erros e logs de execução
    ├── 📄 main.c          # Ponto de entrada do simulador e inicialização das threads
    ├── 📄 map.c           # Gerenciamento da malha viária e movimentação atômica
    ├── 📄 render.c        # Atualização visual da tela e interface do terminal
    ├── 📄 traffic_light.c # Sincronização dos tempos de abertura/fechamento das vias
    └── 📄 vehicle.c       # Funções para veículos deslocamento no mapa, criação e destruição

```


## Decisões de implementação 


## Fluxo de execução
 
**Threads em execução simultânea:**
- 1 thread para o relógio (`clock`)
- 1 thread para o renderizador
- 1 thread para os semáforos
- 1 thread para o analisador
- N threads para os veículos (uma por veículo, entre 10 e 20)
**Ciclo de um tick:**
 
1. O clock acorda todas as threads trabalhadoras.
2. Em paralelo:
   - O renderizador exibe o estado do tick anterior.
   - Os veículos calculam sua próxima posição e enviam uma requisição ao analisador.
   - O semáforo calcula o novo estado das luzes (consultando a coordenada de prioridade da ambulância).
3. O semáforo sinaliza ao clock que terminou.
4. Cada thread de veículo dorme aguardando a decisão do analisador.
5. Quando todos os veículos enviaram suas requisições, o analisador é acionado para validá-las.
   - Cada veículo possui um slot exclusivo indexado pelo seu id.
   - Em caso de conflito de destino, o veículo de menor id é aprovado.
6. O analisador acorda cada thread de veículo com o veredito (aprovado ou negado).
7. Cada veículo atualiza (ou não) seu estado interno e sinaliza ao clock.
8. O renderizador sinaliza ao clock ao terminar de exibir o frame.
9. Após todas as threads sinalizarem, o clock:
   - Realiza o swap dos buffers do analisador e do semáforo (double buffering).
   - Incrementa o tick.
   - Acorda todas as threads para o próximo ciclo.
> A renderização é feita com um tick de atraso: o renderizador lê o buffer inativo do analisador e do semáforo, que contém o estado consolidado do tick anterior. Isso evita concorrência entre escrita e leitura dos buffers.
 
---
 
## Decisões de implementação
 
### Mapa
 
A malha viária é representada por uma matriz de células (`Tile`), onde cada caractere do arquivo de configuração é mapeado para um tipo (`TileType`), indicando o tipo do terreno e a direção de fluxo da via.
 
#### map.h
 
- **`map_new()`**: Aloca recursos, inicializa o mutex interno e carrega o mapa a partir de um arquivo de configuração.
- **`map_get_tile_type()`**: Retorna o tipo de uma célula em uma dada coordenada. Retorna `TILE_BLOCKED` para coordenadas inválidas ou fora dos limites.
- **`map_is_blocked()`**: Verifica se uma célula é permanentemente intransitável. Não considera o estado de ocupação — uma célula ocupada por um veículo continua sendo transitável se seu tipo for diferente de `TILE_BLOCKED`.
- **`map_is_occupied()`**: Verifica se uma célula está ocupada por um veículo ou é do tipo `TILE_BLOCKED`. Leitura thread-safe via mutex interno.
- **`map_transfer_occupant()`**: Transfere a ocupação entre duas células de forma atômica, protegida pelo mutex do mapa. Verifica se o destino é válido, não bloqueado e não ocupado antes de executar a transferência.
- **`map_reserve_spawn_point()`**: Varre a malha viária em busca de uma célula de via direcional livre e a reserva atomicamente para um novo veículo.
#### map.c
 
- **`find_dimensions()`**: Determina largura e altura do mapa percorrendo o arquivo caractere a caractere, ignorando espaços em branco.
- **`fill_tiles()`**: Preenche o grid de tiles a partir do arquivo, mapeando cada caractere para o `TileType` correspondente. Caracteres desconhecidos são tratados como `TILE_BLOCKED`.
- **`tile_at()`**: Função interna que converte uma coordenada 2D em um ponteiro para o tile correspondente no array 1D (layout row-major).
### Threads
 
#### Thread dos veículos — `vehicle_update`
 
Representa o ciclo de vida de um veículo durante a simulação. A cada tick:
1. Valida se o movimento pretendido é permitido (limites, adjacência, regras de ultrapassagem).
2. Envia uma requisição de movimento ao analisador.
3. Aguarda a decisão do analisador (bloqueado em variável de condição, sem busy-waiting).
4. Atualiza posição e direção caso o movimento seja aprovado.
5. Sinaliza ao clock que concluiu o tick.
#### Thread do relógio — `clock_update`
 
Coordena o avanço dos ticks e sincroniza todas as threads trabalhadoras. Aguarda que todas as threads sinalizem conclusão antes de avançar o tick. Realiza o swap dos buffers do analisador e do semáforo antes de acordar as threads para o próximo ciclo. A espera é implementada com variável de condição, sem busy-waiting.
 
#### Thread do analisador — `analyser_update`
 
Árbitro de movimentos da simulação. Permanece bloqueado até que todos os veículos tenham enviado suas requisições (controlado por `pending_count` e `analyser_cond`). Ao ser acionado, varre os slots sequencialmente, valida cada requisição contra o estado do mapa e resolve conflitos de destino (menor id vence). Acorda cada veículo individualmente com o veredito.
 
#### Thread do renderizador — `render_update`
 
Exibe o estado da simulação no terminal a cada tick. No tick 0, realiza um redesenho completo do mapa. Nos ticks seguintes, opera em modo seletivo: atualiza apenas as células afetadas por movimentos aprovados no tick anterior, lidos do buffer inativo do analisador. Utiliza double buffering e códigos de escape ANSI para evitar flickering.
 
#### Thread dos semáforos — `traffic_light_update`
 
Gerencia o ciclo de abertura e fechamento das vias em todas as interseções. A cada tick, consulta a coordenada de prioridade da ambulância via `vehicle_get_priority_coord()` e atualiza o estado alvo das luzes. As transições de estado sempre passam por `YELLOW` (nunca `GREEN → RED` diretamente). Sinaliza ao clock ao concluir o processamento do tick.
 
### Mecanismos de sincronização
 
#### Sincronização por clock (barreira global)
 
O módulo `Clock` atua como coordenador temporal da simulação. Todas as threads trabalhadoras (veículos, analisador, renderizador e semáforo) sinalizam ao clock ao concluir o processamento de cada tick via `clock_signal()`. O clock só avança quando todas sinalizaram, garantindo que nenhuma thread inicie o próximo tick antes que as demais concluam o atual.
 
#### Variáveis de condição
 
- **Veículos aguardando o analisador**: cada veículo bloqueia em `slot_cond[id]` após enviar sua requisição, sem consumir CPU.
- **Analisador aguardando os veículos**: o analisador bloqueia em `analyser_cond` até `pending_count == VEHICLE_COUNT`.
- **Threads aguardando o clock**: todas as threads bloqueiam em `cond_vehicles` após sinalizar ao clock, acordando somente quando o tick avança.
#### Mutex por módulo
 
- **Mapa**: mutex único protege todas as leituras e escritas de `is_occupied`, garantindo atomicidade em `map_transfer_occupant` e `map_is_occupied`.
- **Semáforo**: mutex único protege o acesso ao estado das luzes entre a thread do semáforo e as threads dos veículos.
- **Analisador**: mutex por slot protege a escrita/leitura de cada requisição individualmente; mutex global do analisador protege `pending_count` e `active_request`.
### Ausência de espera ocupada
 
Nenhuma thread verifica repetidamente o estado da simulação em loop. Toda espera é implementada com `pthread_cond_wait`, que bloqueia a thread sem consumir CPU até que o evento esperado ocorra.
 
### Prevenção de deadlocks
 
#### Centralização das decisões no analisador
 
Os veículos não alteram diretamente o mapa. Todos enviam uma requisição ao analisador e aguardam sua decisão, eliminando a necessidade de cada veículo adquirir locks de células individualmente.
 
#### Mutex único por recurso compartilhado
 
Cada operação crítica adquire no máximo um mutex por vez, eliminando a possibilidade de espera circular — uma das quatro condições necessárias para a ocorrência de deadlock.
 
#### Ordem de aquisição de locks no analisador
 
No módulo `analyser`, a ordem de aquisição é sempre: `slot_mutex[id]` → liberado → `analyser_mutex` → liberado → `slot_mutex[id]`. Os dois mutexes nunca são mantidos simultaneamente pela mesma thread, prevenindo deadlock circular entre veículos e o analisador.
 
#### Sincronização por ticks
 
Todas as threads executam exatamente uma etapa por tick e só prosseguem após a sincronização do clock, evitando que threads avancem enquanto outras ainda manipulam estruturas compartilhadas.
 
#### Validações antes do acesso ao recurso compartilhado
 
Antes de enviar uma requisição ao analisador, cada veículo realiza verificações locais:
 
```c
if (!map_is_within_bounds(map, target))     { /* movimento inválido */ }
else if (!is_adjacent(map, vehicle, target)) { /* movimento inválido */ }
else if (is_overtaking(map, vehicle, target)){ /* movimento inválido */ }
else { /* envia requisição ao analisador */ }
```
 
Isso reduz o número de conflitos que precisam ser resolvidos pelo analisador.
 
---
 
## Membros e responsabilidades
 
| Nome | Responsabilidades |
|------|-------------------|
| Gabriel Souza Santos | Implementação do clock, definição da estrutura do projeto, renderização, debug e revisão de código. |
| José Dhonatan Fernandes de Almeida | Criação do mapa, funções auxiliares de veículo, implementação do semáforo sem espera ocupada. |
| Letícia Maria dos Santos Dias | Implementação dos veículos, issues e divisão de responsabilidades, relatório técnico e documentação. |
| Sarah Mendes Teles | Implementação da ambulância, sistema de prioridade e estratégias de prevenção de deadlocks. |
