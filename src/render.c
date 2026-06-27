/**
 * @file render.c
 * @brief Renderização ASCII da malha urbana.
 */


#include "render.h"


#include <stdio.h>


#include "map.h"
#include "vehicle.h"


/*
 * Converte um tipo de célula para um caractere ASCII.
 * Quando surgirem cruzamentos e semáforos basta adicionar
 * novos cases neste switch.
 */
static char tile_to_char(TileType tile)
{
    switch(tile)
    {
        case BLOCKED:
            return '#';


        case ROAD_UP:
        case ROAD_DOWN:
            return '|';


        case ROAD_LEFT:
        case ROAD_RIGHT:
            return '-';


        default:
            return '?';
    }
}


/*
 * Limpa a tela utilizando códigos ANSI.
 */
void clear_screen(void)
{
    printf("\033[2J");
    printf("\033[H");
}


/*
 * Desenha toda a simulação.
 */
void render_frame(unsigned long tick)
{
    clear_screen();


    printf("=============================================\n");
    printf("      SIMULADOR DE TRAFEGO URBANO\n");
    printf("=============================================\n");
    printf("Tick: %lu\n\n", tick);


    for(int y = 0; y < MAP_HEIGHT; y++)
    {
        for(int x = 0; x < MAP_WIDTH; x++)
        {
            char symbol = tile_to_char(map[y][x].tile);


            /*
             * Verifica se existe algum veículo nesta posição.
             */
            for(int i = 0; i < VEHICLE_COUNT; i++)
            {
                if(cars[i].x == x &&
                   cars[i].y == y)
                {
                    if(cars[i].type == AMBULANCE)
                    {
                        symbol = 'A';
                    }
                    else
                    {
                        symbol = '0' + (cars[i].id % 10);
                    }


                    break;
                }
            }


            putchar(symbol);
        }


        putchar('\n');
    }


    printf("\nLegenda\n");
    printf("-----------------------------\n");
    printf("# = Bloco\n");
    printf("| = Rua Vertical\n");
    printf("- = Rua Horizontal\n");
    printf("0-9 = Carros\n");
    printf("A = Ambulancia\n");


    fflush(stdout);
}
