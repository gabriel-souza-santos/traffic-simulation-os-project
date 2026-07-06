#include <stdlib.h>
#include "simulation.h"

int main(int argc, char *argv[]) {
    Simulation *simulation = simulation_new();
    simulation_run(simulation);
    simulation_destroy(simulation);
    return EXIT_SUCCESS;
}