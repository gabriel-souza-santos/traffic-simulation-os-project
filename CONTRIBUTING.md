# Regras de Contribuição

## Convenções de nomenclatura

Este projeto adota as seguintes convenções de nomenclatura:

| Elemento  | Convenção          | Exemplo           |
| --------- | ------------------ | ----------------- |
| Variáveis | `snake_case`       | `current_tick`    |
| Funções   | `snake_case`       | `clock_get_tick`  |
| Structs   | `PascalCase`       | `Clock`           |
| Enums     | `PascalCase`       | `ClockState`      |
| Typedefs  | `PascalCase`       | `ClockHandle`     |
| Macros    | `UPPER_SNAKE_CASE` | `MAX_BUFFER_SIZE` |

Não utilize `camelCase` ou misturas como `snake_camelCase` para identificadores de variáveis ou funções.

---

## Documentação

Toda a documentação do código deve utilizar **Doxygen**.

* As docstrings das **funções públicas** devem ser escritas no arquivo de cabeçalho (`.h`).
* As docstrings das **funções privadas** devem ser escritas no arquivo de implementação (`.c`), quando necessário.
* Estruturas, enums, typedefs e macros públicas também devem ser documentadas no cabeçalho correspondente.
* Não repita a documentação de uma função pública caso ela já tenha sido feita no header (`.h`).

---

## Organização dos módulos

Como C não possui namespaces nem classes, funções públicas devem ser prefixadas com o nome do módulo ao qual pertencem para evitar colisões de nomes
e ajudar na organização dos módulos.

Por exemplo, no módulo `clock`:

```c
clock_new();
clock_destroy();
clock_get_tick();
```

Essa mesma convenção é amplamente utilizada por bibliotecas da linguagem C, como a POSIX Threads:

```c
pthread_create();
pthread_join();
pthread_mutex_lock();
```

---

## Funções públicas e privadas

A localização da declaração da função define sua visibilidade:

* Se a assinatura da função estiver presente em um arquivo `.h`, ela é considerada **pública** e pode ser utilizada por outros módulos.
* Se a função for declarada apenas no arquivo `.c`, ela é considerada **privada** ao módulo.

Para funções privadas:

* recomenda-se utilizar o especificador `static`, indicando ausência de *external linkage*;
* o prefixo do módulo é opcional. Recomenda-se omiti-lo para manter os identificadores menores e reforçar a distinção entre a API pública e a implementação interna.

Exemplo:

```c
/* clock.h */
Clock *clock_new(void);
void clock_destroy(Clock *clock);
int clock_get_tick(const Clock *clock);
```

```c
/* clock.c */

static void update_tick(Clock *clock)
{
    ...
}

static bool is_running(const Clock *clock)
{
    ...
}
```

---

## API pública

O arquivo `.h` representa a interface pública do módulo.

Procure expor apenas as funções necessárias para que outros módulos utilizem a biblioteca. Funções auxiliares e detalhes de implementação devem permanecer restritos ao arquivo `.c`.

Isso reduz o acoplamento entre módulos e facilita futuras alterações na implementação sem quebrar a API pública.

---

## Include Guards

Todo arquivo de cabeçalho (`.h`) deve utilizar *include guards* para evitar múltiplas inclusões.

O nome da macro deve seguir o padrão:

```c
#ifndef URBAN_TRAFFIC_<FILE_NAME>_H
#define URBAN_TRAFFIC_<FILE_NAME>_H

...

#endif // URBAN_TRAFFIC_<FILE_NAME>_H
```

Onde `<FILE_NAME>` corresponde ao nome do arquivo convertido para `UPPER_SNAKE_CASE`.

Exemplos:

Arquivo `clock.h`:

```c
#ifndef URBAN_TRAFFIC_CLOCK_H
#define URBAN_TRAFFIC_CLOCK_H

...

#endif // URBAN_TRAFFIC_CLOCK_H
```

Arquivo `traffic_light.h`:

```c
#ifndef URBAN_TRAFFIC_TRAFFIC_LIGHT_H
#define URBAN_TRAFFIC_TRAFFIC_LIGHT_H

...

#endif // URBAN_TRAFFIC_TRAFFIC_LIGHT_H
```

