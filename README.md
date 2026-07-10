# Simulador de Tráfego Urbano em C

O Simulador de tráfego urbano é um projeto de sistema desenvolvido em C utilizando threads, mutexes, semáforos e variáveis de condição
para demonstrar conceitos de Sistemas Operacionais como sincronização, exclusão mútua, coordenação de threads
e prevenção de deadlocks.

--

## Como executar

Devido ao uso da biblioteca Posix Threads, a compatilbilidade desse projeto é limitada

compilação é feita via CMake

recomenda-se criar um pasta build para gerar o cmake,

cmake -S . build

compila o programa

cmake --build build

executa

./build/bin/urban_trafic


ou diretamente:
cmake -S . build && cmake --build build && ./build/bin/urban_trafic


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
├── 📁 res
│   └── 📄 map.txt         # Arquivo de configuração textual da matriz do mapa
│
└── 📁 src
    ├── 📄 clock.c         # Implementação da sincronização baseada em turnos 
    ├── 📄 deadlock.c      # Algoritmos de tratamento de impasses entre threads
    ├── 📄 debug.c         # Gerenciamento de saída de erros e logs de execução
    ├── 📄 main.c          # Ponto de entrada do simulador e inicialização das threads
    ├── 📄 map.c           # Gerenciamento da malha viária e movimentação atômica
    ├── 📄 render.c        # Atualização visual da tela e interface do terminal
    └── 📄 traffic_light.c # Sincronização dos tempos de abertura/fechamento das vias
    └── 📄 vehicle.c       # Funções para veículos deslocamento no mapa, criação e destruição

```

nova estrutura 
├── include
│   ├── analyser.h
│   ├── clock.h
│   ├── debug.h
│   ├── map.h
│   ├── render.h
│   ├── simulation.h
│   ├── traffic_light.h
│   └── vehicle.h
├── out
│   └── debug.log
├── res
│   ├── data
│   │   └── map-data.txt
│   ├── tile
│   │   ├── tile-blocked.txt
│   │   └── tile-road.txt
│   ├── traffic-light
│   │   ├── light-green.txt
│   │   ├── light-red.txt
│   │   └── light-yellow.txt
│   └── vehicle
│       ├── ambulance-left.txt
│       ├── ambulance-right.txt
│       ├── car-fast-left.txt
│       ├── car-fast-right.txt
│       ├── car-medium-left.txt
│       ├── car-medium-right.txt
│       ├── car-slow-left.txt
│       └── car-slow-right.txt
└── src
├── analyser.c
├── clock.c
├── debug.c
├── main.c
├── map.c
├── render.c
├── simulation.c
├── traffic_light.c
└── vehicle.c


## Decisões de implementação 


## Fluxo de execução

Treads:
1 thread para o clock
1 thread para o renderizador
1 thread para o semáforo / luzes de trâsito
1 thread para o analisador
N threads para os veículos (uma para cada), respeitando os valores mínimos de 10 a maximos de 20 veículos

- Clock acorda as treads trabalhadoras
- Cock dorme até que terminem
- renderizador escreve o tick anterior / veiculos calculam sua proxima posição / semaforos calculam o estado das luzes
  (aqui o renderizador opera de forma independendte, mas os veículos precisam receber a informação da luz do semáforo,
 e o semáforo precisa saber qual a coodenada de prioridade da ambulancia, o controle disso é feito via mutex único para 
 acesso ás luzes)
- ao terminar de atualizar as luzes traffic_light sinaliza ao clock
- ao calcular sua posição e thread do veículo dorme e aguarda pelo analisador
- quando todos os veículos mandam suas requisições, o analisador é acionado para validá-las
- cada veículo tem um slot específico para mandar a requisição, id 0 no primeiro slot, id 1 no segundo etc
- se dois veículos competem pelo mesmo espaço, ganha aquele com menor id
- ao aprovar ou recusar um requisição, o analisador acorda a thread do veículo
- que atualiza ou não seu estado interno a depender do veredito
- ao terminar a análise o clock é sinalizado
- ao terminar a atulização do estado interno os veículos sinalizam ao clock
- quando o renderizador termina sinaliza ao clock
- após todas terem terminado, o clock faz o swap do buffers do analisador e do semáforo ( utiliza-se um buffer duplo
 um ativo que está sendo escrito no tick atual e um inativo que contém dados do estdo anterior e é usado pelo renderizador)
- em seguida incrementa o valor do tick e acorda as threads trabalhadoras

A renderização do mapa é feita de forma atrasada, o contexto do tick anterior é salvo
em analyser, que guarda as informações das requisições feitas, e portanto sabe quais
veiculos se moveram ou não e suas posições.

mapa usa apenas um único mutex por simplicidade, 

### Mapa

### Threads

### Mecanismos de sicronização

### Ausência de espera ocupada

### Prevenção de deadlocks

## Membros e responsabilidades

| Nome | Responsabilidades |
|------|-------------------|
| Gabriel Souza Santos | Implementação do clock, definição de estrutura do projeto, renderização, debug e revisão do código. |
| José Dhonatan Fernandes de Almeida | Criação do mapa, funções auxiliares de veículo, implementação do semáforo sem espera ocupada. |
| Letícia Maria dos Santos Dias | Implementação dos veículos normais, issues e divisão de responsabilidades, relatório técnico e documentação. |
| Sarah Mendes Teles | Implementação da ambulância, sistema de prioridade e mecanismos para prevenção de deadlocks. |
