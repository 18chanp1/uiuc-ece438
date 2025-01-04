#include<stdio.h>
#include<string.h>
#include<stdlib.h>

#include <iostream>

#include "CSMASimulator.h"

int main(int argc, char** argv) {
  //printf("Number of arguments: %d", argc);
  if (argc != 2) {
      printf("Usage: ./csma input.txt\n");
      return -1;
  }

  std::string filename(argv[1]);
  CSMASimulator csma_simulator(filename);

  double goodput = csma_simulator.simulate();

  //output to file
  std::ofstream out("output.txt");

  out << std::fixed << std::setprecision(2) << goodput;
  out.flush();

    return 0;
}

