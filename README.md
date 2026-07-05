# Simulador de Tráfego Urbano em C

O Simulador de tráfego urbano é um projeto de sistema desenvolvido em C utilizando threads, mutexes, semáforos e variáveis de condição
para demonstrar conceitos de Sistemas Operacionais como sincronização, exclusão mútua, coordenação de threads
e prevenção de deadlocks.

--

## Como executar

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

## Decisões de implementação 

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
