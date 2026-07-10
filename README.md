# Simulador de Tráfego Urbano em C

O Simulador de tráfego urbano é um projeto de sistema desenvolvido em C utilizando threads, mutexes, semáforos e variáveis de condição
para demonstrar conceitos de Sistemas Operacionais como sincronização, exclusão mútua, coordenação de threads
e prevenção de deadlocks.

--

## Como executar
### 1. Instalar dependências
```
 wsl --install
 ```

### 2. Como rodar o projeto
Após a instalção, no terminal ou powershell digite:
```
wsl
```
E depois:
```
cmake -S . build && cmake --build build && ./build/bin/urban_traffic
```

--
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
│    └── 📄 debug.log               # Arquivo em caso de erros
│
├── 📁 res
│   └── 📁 Data                     # Diretório da reepresentação visual do mapa
│    │   └── 📄 map.txt             # Representação visual da rua
│    ├── 📁 Tile                    # Diretório da reepresentação visual dos elementos da rua
│    │   ├── 📄 tile-blocked.txt    # Representação visual da casa
│    │   └── 📄 tile-road.txt       # Representação visual da calçada
│    ├── 📁 Traffic-light           # Diretório da reepresentação visual dos semáforos
│    │   ├── 📄 light-green.txt     # Representação visual do semaforo verde
│    │   ├── 📄 light-red.txt       # Representação visual do semaforo vermelho
│    │   └── 📄 light-yellow.txt    # Representação visual do semaforo amarelo
│    │
│    └── 📁 Vehicle                      # Diretório da reepresentação visual dos veículos
│         ├── 📄 ambulance-right.txt     # Representação visual da ambulância (prioridade)
│         ├── 📄 ambulance-left.txt     # Representação visual da ambulância (prioridade)
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

### Mapa
A primeira escolha que tivemos que fazer ao iniciar o desenvolvimento foi como estruturar o mapa e a solução acima foi a que escolhemos, consideramos cada tipo de caractere como um tipo de objeto do mundo real, seja estrada, semáforo ou casa. 
#### mapa.h
O arquivo mapa.h definido no diretório include traz uma definição da interface pública do mapa e sua manipulação de coordenadas.
- **map_new() :** Essa função aloca recursos, inicializa recursos internos e carrega o mapa.

- **map_get_tile_type():** Obtém o tipo de uma célula numa dada coordenada.
Utiliza como parâmetros um ponteiro(*map) para a estrutura do mapa e a estrutura de coordenada contendo as posições X e Y desejadas. A função retorna o valor correspondente do enum TileType. Se o mapa for inválido ou a coordenada estiver fora dos limites do mapa, retorna TILE_BLOCKED.

-  **map_is_blocked():** Checa se uma célula é permanentemente intransitável.
Esta função verifica apenas o tipo da célula, não considerando seu estado de ocupação. Assim, uma célula ocupada por um veículo continua sendo considerada transitável caso seu tipo seja diferente de TILE_BLOCKED.

- **map_is_occupied():** Checa se uma célula está ocupada no momento por um veículo ou obstáculo físico. Esta verificação realiza uma leitura thread-safe utilizando o mecanismo interno de sincronização do mapa. Células são consideradas ocupadas se houver um veículo nelas ou se forem do tipo TILE_BLOCKED.

- **map_transfer_occupant():** Realiza a transferência de um ocupante entre duas células, verifica se a célula de destino é válida, não está bloqueada e não se encontra ocupada. Caso a movimentação seja permitida, a ocupação é removida da célula de origem é atribuída à célula de destino como uma operação atômica protegida pelo mecanismo interno de sincronização do mapa.

- **map_reserve_spawn_point():** Procura uma posição inicial disponível para um novo veículo e a reserva. Varre a malha viária em busca de uma célula pertencente a uma via direcional válida(TILE_ROAD_UP, TILE_ROAD_DOWN, TILE_ROAD_LEFT ou TILE_ROAD_RIGHT) que não esteja ocupada. 

#### Mapa.c
O arquivo mapa.c faz a implementação da lógica completa do mapa, esse arquivo está no diretório src e as suas funções são:

- **fill_tiles()**: Responsável por preencher as células do mapa a partir do conteúdo do arquivo. Percorre o arquivo aberto, ignorando quebras de linha e espaços, mapeando cada caractere válido para o TileType correspondente.

- **map_new():** Implementação da construção do mapa a partir de arquivo. O carregamento ocorre em duas passagens pelo arquivo: primeiro calcula as dimensões do mapa com find_dimensions(), depois aloca a estrutura e o array de tiles com calloc(), garantindo o estado padrão

- **map_get_tile_type():** Implementação da consulta de tipo de tile. Realiza verificação de limites com map_is_within_bounds(). Caso a coordenada esteja fora dos limites do mapa ou o ponteiro do mapa seja NULL, retorna TILE_BLOCKED.

-  **map_is_blocked():**  Implementação da verificação de célula bloqueada.
Verifica apenas o tipo da célula, retornando verdadeiro quando tile->type == TILE_BLOCKED. 

- **map_is_occupied():** Implementação da verificação de ocupação. Valida os limites antes de qualquer acesso (retornando true para posições fora do mapa, por segurança). 

- **map_transfer_occupant():** Implementação da transferência de ocupação entre duas células. Adquire o mutex global do mapa antes de inspecionar ou alterar qualquer tile, garantindo que a verificação de disponibilidade do destino e a atualização das células de origem e destino ocorram como uma operação atômica.

- **map_reserve_spawn_point():** Implementação da busca e reserva de ponto de spawn.
Mantém o mutex global do mapa adquirido durante toda a varredura linear do grid (linha por linha), retornando antecipadamente que
esteja livre.


### Threads
No projeto utilizamos threads para representar os veículos, uma para o relógio, uma para o analisador e uma para o renderizador. Abaixo estão as threads identificadas e o papel de cada uma.
#### Threads dos veículos
Presente em src/vehicle.c a função que executa como thread de veículos é a seguinte:
```
void *vehicle_update(void *vehicle_args)
```
Ela representa o ciclo de vida de um veículo durante a simulação. Cada veículo criado pelo programa executa essa função em uma thread independente. Durante cada tick da simulação, ela:
- valida se o movimento é permitido;
- envia uma requisição ao analisador;
- aguarda a decisão do analisador;
- atualiza sua posição e direção caso o movimento seja aprovado;
- sincroniza-se com o relógio para iniciar o próximo tick.



#### Thread do relógio (Clock)

A função clock_update presente em src/clock.c implementa o laço principal da thread responsável pelo relógio global da simulação. Sua função é coordenar o avanço dos ticks e sincronizar todas as threads trabalhadoras.
A espera é implementada utilizando um laço while, garantindo que despertares das variáveis de condição não comprometam a sincronização entre as threads.


#### Thread do analisador
A função analyser_update presente no arquivo src/analyser.c, implementa o laço principal da thread responsável pelo analisador de movimentos da simulação. Sua finalidade é atuar entre as threads dos veículos e o estado compartilhado do mapa, garantindo que as movimentações ocorram de forma consistente e livre de conflitos.

Durante cada tick da simulação, a thread permanece bloqueada até que todos os veículos tenham enviado suas respectivas requisições de movimento. Essa sincronização é realizada por meio da variável de condição analyser_cond e do contador pending_count, assegurando que a análise só seja iniciada quando todas as solicitações do tick atual estiverem disponíveis.

#### Thread do Renderizador
A thread do renderizador é responsável por exibir, a cada tick da simulação, o estado atual do mapa e dos veículos no terminal. Sua execução ocorre de forma sincronizada com as demais threads por meio do relógio global, garantindo que a visualização corresponda ao estado da simulação em cada instante.

### Mecanismos de sicronização
#### Sincoronização por Clock
O módulo Clock funciona como coordenador do tempo da simulação. Cada thread possui uma participação na barreira de sincronização, incluindo:
- Threads dos veículos;
- Thread do analisador;
- Thread do renderizador;
- Outros componentes sincronizados da simulação.

#### Sincronização entre Threads
Cada thread possui um ciclo semelhante:

**Veículos**
1. Executa movimento
2.  Envia resultado ao Clock
3. Aguarda próximo tick

**Renderizador**
1. Renderiza estado atual
2. Sinaliza conclusão ao Clock
3. Aguarda próximo tick

**Analisador**
1. Processa requisições
2. Sinaliza conclusão ao Clock
3. Aguarda próximo tick

Nenhuma dessas threads precisa manter um laço consumindo CPU enquanto espera.


### Ausência de espera ocupada
A nossa simulação evita o uso de espera ocupada através de mecanismos de sincronização entre threads, utilizando principalmente barreiras baseadas em condição de execução e comunicação entre os módulos por meio do Clock.

Na implementação, as threads não ficam verificando constantemente o estado da simulação. Cada componente executa sua tarefa apenas quando necessário e, ao finalizar seu processamento referente ao tick atual, informa sua conclusão ao relógio global utilizando a função: ```clock_signal(clock, current_tick);```

-  **Espera por Eventos em vez de Verificação Contínua:** A comunicação entre veículos e o analisador também evita espera ocupada. Quando um veículo deseja se movimentar, ele não tenta alterar diretamente o mapa nem fica verificando repetidamente se sua solicitação foi aceita.

- **Uso de Mutex para Controle de Concorrência:** Além da sincronização temporal, o sistema utiliza exclusão mútua para evitar conflitos no acesso a estruturas compartilhadas. 


### Prevenção de deadlocks
#### Centralização das decisões no Analyser
Na nossa simulação, os veículos não alteram diretamente o mapa. Em vez disso, todos enviam uma requisição ao Analyser e aguardam sua decisão.
```bash
// Monta a requisição e envia ao Analisador
const MovementRequest request = {
    .from = current_position,
    .to = target_position,
    .status = REQUEST_PENDING,
};

// Bloqueia a thread do veículo até que o Analisador decida por todos
analyser_request(analyser, id, request);
const RequestStatus verdict = analyser_get_status(analyser, id);
```
Em vez de cada thread tentar reservar ou bloquear diretamente as posições do mapa, todos os veículos apenas enviam uma requisição contendo sua posição atual e o destino pretendido.

#### Uso de um único mutex para recursos compartilhados
Essa estratégia impede que uma thread adquira vários mutexes simultaneamente, eliminando a possibilidade de espera circular, uma das quatro condições necessárias para ocorrência de deadlock. Como cada operação crítica utiliza apenas um bloqueio por vez, não existe dependência entre múltiplos recursos.
#### Sincronização por Ticks
Todas as threads executam somente uma etapa da simulação por tick e somente prosseguem após a sincronização realizada pelo relógio global. Esse modelo evita que algumas threads avancem enquanto outras ainda manipulam estruturas compartilhadas, e isso reduz a possibilidade de bloqueios entre operações concorrentes.
#### Validações antes do acesso ao recurso compartilhado
Antes de solicitar qualquer movimentação ao analisador, cada veículo realiza diversas verificações locais, como é possivel verificar abaixo:
```if (!map_is_within_bounds(map, intended_target)) {  ...}
else if (!is_adjacent(map, vehicle, intended_target)) {  ...}
else if (is_overtaking(map, vehicle, intended_target)) {   ...}
else { target_position = intended_target;}
```
Essas verificações detectam movimentos inválidos antes que eles sejam enviados ao Analyser e diminuem o número de conflitos concorrentes que precisam ser resolvidos pelo sistema.



## Membros e responsabilidades

| Nome | Responsabilidades |
|------|-------------------|
| Gabriel Souza Santos | Implementação do clock, definição de estrutura do projeto, renderização, debug e revisão do código. |
| José Dhonatan Fernandes de Almeida | Criação do mapa, funções auxiliares de veículo, implementação do semáforo sem espera ocupada. |
| Letícia Maria dos Santos Dias | Implementação dos veículos normais, issues e divisão de responsabilidades, relatório técnico e documentação. |
| Sarah Mendes Teles | Implementação da ambulância, sistema de prioridade e mecanismos para prevenção de deadlocks. |
