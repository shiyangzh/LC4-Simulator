/*
 * loader.c : Defines loader functions for opening and loading object files
 */

#include <stdio.h>
#include "loader.h"
#include <string.h>

// memory array location
unsigned short memoryAddress;

/*
 * Read an object file and modify the machine state as described in the writeup
 */
int ReadObjectFile(char* filename, MachineState* CPU) {
  FILE *my_file;
  unsigned short int n;
  unsigned short int hex [1];
  
  my_file = fopen(filename, "rb");
  if (my_file == NULL) { // return error if invalid file name
    printf("error: ReadObjectFile() failed\n");
    return 1;
  }

  while (fread(hex, sizeof(unsigned short int), 1, my_file)) {
    if (hex[0] == 0xDECA) { // check if word is header
      fread(hex, sizeof(unsigned short int), 1, my_file);
      memoryAddress = (hex[0]>>8) | (hex[0]<<8); // get memory address
      fread(hex, sizeof(unsigned short int), 1, my_file);
      n = (hex[0]>>8) | (hex[0]<<8); // get n-word body
      while (n > 0) { // write instructions in memory
        fread(hex, sizeof(unsigned short int), 1, my_file);
        CPU->memory[memoryAddress] = (hex[0]>>8) | (hex[0]<<8);
        memoryAddress++;
        n--;
      }
    } else if (hex[0] == 0xDADA) { // check if word is header
      fread(hex, sizeof(unsigned short int), 1, my_file);
      memoryAddress = (hex[0]>>8) | (hex[0]<<8); // get memory address
      fread(hex, sizeof(unsigned short int), 1, my_file);
      n = (hex[0]>>8) | (hex[0]<<8); // get n-word body
      while (n > 0) { // write instructions in memory
        fread(hex, sizeof(unsigned short int), 1, my_file);
        CPU->memory[memoryAddress] = (hex[0]>>8) | (hex[0]<<8);
        memoryAddress++;
        n--;
      }
    } 
  }
  return 0;
}
