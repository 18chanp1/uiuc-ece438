//
// Created by Beako on 2024.11.21.
//

#include "CSMASimulator.h"

#include <iostream>


/**
 * Initialize simulator object
 * @param inputFileName file containing the input data as specified in mp doc
 */
CSMASimulator::CSMASimulator(std::string inputFileName) {
  // open the file
  std::fstream inputFile(inputFileName);

  //check file opened successfully
  if(!inputFile.is_open()) {
    throw std::invalid_argument("File not found");
  }

  while(inputFile) {
    std::string currLine;
    getline(inputFile, currLine);

    //if line length is blank, skip
    if(currLine.length() == 0) {
      continue;
    }

    //get the parameter type
    char c = currLine[0];

    //check that c is valid
    if(c != 'N' && c != 'L' && c != 'M' && c != 'R' && c != 'T') {
      throw std::invalid_argument("Invalid parameter type");
    }

    //get the list of arguments
    std::vector<int> toks = parseMultipleArgs(c, currLine);

    switch(c) {
      case 'N': {
        this->node_count = toks[0];
        break;
      }
      case 'L': {
        this->packet_sz = toks[0];
        break;
      }
      case 'M': {
        this->retransmissions = toks[0];
        break;
      }
      case 'R': {
        //check retransmission vector is equal to number of retranmissions
        if(this->retransmissions != -1 && toks.size() != this->retransmissions) {
          throw std::invalid_argument("Invalid number of retransmissions");
        }
        this->retransmission_backoff = toks;
        break;
      }
      case 'T': {
        this->simulation_time = toks[0];
        break;
      }
      default: {
        throw std::invalid_argument("Invalid parameter type");
      }
    }
  }

  //done reading from input file.
  std::cout << "Done reading from input file" << std::endl;

}

/**
 *
 * @param c The parameter type
 * @param currLine The line from which it is parsed
 * @return the value parsed
 * @throw Invalid argument if the parameter is not valid
 */
std::vector<int> CSMASimulator::parseMultipleArgs(char c, std::string currLine) {
  std::stringstream ss(currLine);
  std::string tok;

  std::vector<int> toks;

  int count = 0;

  while(ss >> tok) {
    if(count != 0) {
      int tok_intval = std::stoi(tok);

      if(tok_intval < 0) {
        throw std::invalid_argument("Invalid argument value");
      }

      toks.push_back(tok_intval);
    }
    count++;
  }

  //throw exception if the count is not correct
  if(count < 2) {
    throw std::invalid_argument("Invalid number of arguments");
  }

  return toks;
}

/**
 * Simulates one single round of the CSMA protocol, updating the state of the
 * simulator.
 */
void CSMASimulator::simulateRound() {
  //if a transmission is ongoing and has not finished, decrement the time remaining
  if(transmissionRemaining > 0) {
    if(transmissionRemaining == 1) {
      //we need to reset the backoffs for the node
      this->currBackoffIndex[transmittingNode] = 0;
      this->ticksToTransmission[transmittingNode] = ((currTime + 1) + transmittingNode) % retransmission_backoff[0];
      transmittingNode = -1;
      // std::cout << "Packet transmitted successfully" << std::endl;
    }

    //decrement the transmission remaining, and mark a sucessful transmission timeslot.
    successfulTransmissionTicks++;
    transmissionRemaining--;
    return;
  }


  //iterate through all nodes to check if any or multiple nodes transmit next round.
  int transmissions = static_cast<int>
  (std::count(ticksToTransmission.begin(), ticksToTransmission.end(), 0));

  bool willCollide = transmissions > 1;
  bool willStartTransmissionNextRd = transmissions > 0;

  //if a collision will occur, we update backoff values
  if(willCollide) {
      for(int i = 0; i < this->node_count; i++) {
        bool nodeWillStart = this->ticksToTransmission[i] == 0;

        if(nodeWillStart) {
          int nodeBackoffIndex = this->currBackoffIndex[i];

          //if backoff value is last one, packet is lost. Reset backoff to init value.
          if(nodeBackoffIndex == this->retransmission_backoff.size() - 1) {
            // std::cout << "Packet lost for node " << i << std::endl;
            this->currBackoffIndex[i] = 0;
          }
          // increment backoff index.
          else {
            this->currBackoffIndex[i]++;
          }

          //update the ticks to transmission to incremented backoff val for next round;
          ticksToTransmission[i] = ((currTime + 1) + i) % retransmission_backoff[currBackoffIndex[i]];

        }
        //other nodes are untouched.
      }
  }
  //there is no collision, only a single transmission
  else if(willStartTransmissionNextRd) {
    //find the node that will start transmission
    auto it = std::find(ticksToTransmission.begin(), ticksToTransmission.end(), 0);
    int indexOfNodeTransmitting = it - ticksToTransmission.begin();
    transmittingNode = indexOfNodeTransmitting;
    transmissionRemaining = this->packet_sz - 1;

    //update successful transmission count. Take into account if the
    //transmission will end after simulation time
    if(currTime + 1 <= simulation_time) {
      successfulTransmissionTicks++;
    }
  }
  //no nodes will start transmission, so update tick counts
  else {
    //update the ticks to transmission for all nodes
    for(int i = 0; i < this->node_count; i++) {
      this->ticksToTransmission[i]--;
    }
  }

}


/**
 * Simulates simulation_time rounds of the CSMA protocol, initializing before and updating
 * the state of the simulator for each round.
 * @return the goodput of the simulation
 */
double CSMASimulator::simulate() {

  //initialize backoffMap
  for(int i = 0; i < this->node_count; i++) {
    ticksToTransmission.push_back(i % retransmission_backoff[0]);
    currBackoffIndex.push_back(0);
  }

  //initialize other variables
  this->currTime = 0;
  transmissionRemaining = 0;
  transmittingNode = -1;
  successfulTransmissionTicks = 0;


  //simulate the rounds
  while(this->currTime < this->simulation_time) {
    printState();
    simulateRound();
    this->currTime++;
  }

  printState();

  // compute the goodput
  return (double) successfulTransmissionTicks / (double) simulation_time;
}

/**
 * Prints the current state of the simulator, handy for debugging
 */
void CSMASimulator::printState() {
  // //output the current time
  // std::cout << "Time: " << this->currTime << std::endl;
  // //output the ticks to transmission;
  // for(int i = 0; i < this->node_count; i++) {
  //   std::cout << "Node " << i << " ticks to transmission: " << this->ticksToTransmission[i] << std::endl;
  // }
  //
  // //print transmitter and transmission remaining
  // std::cout << "Transmitting node: " << this->transmittingNode << std::endl;
  //
  // //print successful transmission ticks
  // std::cout << "Successful transmission ticks: " << this->successfulTransmissionTicks << std::endl;
}