/*
 * trace.c: location of main() to start the simulator
 */

#include <stdio.h>
#include "loader.h"

// Global variable defining the current state of the machine
MachineState* CPU;

int main(int argc, char** argv) {
    int i;
    FILE *output;
    MachineState state;
    CPU = &state;
    int test;
    Reset(CPU);
    
    if (argc < 3) { // if there isn't at least 3 arguments
	  printf("invalid arguments\n");
      return -1;
    }

    output = fopen(argv[1], "w");
    for (i = 2; i < argc; i++) { // read each file in argument
        test = ReadObjectFile(argv[i], CPU);
        if (test == -1) {
            return -1;
        }
    }

    while (CPU->PC != 0x80FF) {
        if (UpdateMachineState(CPU, output) == 1) {
            break;
        }
    }

    return 0;
}