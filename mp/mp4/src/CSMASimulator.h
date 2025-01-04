//
// Created by Beako on 2024.11.21.
//

#ifndef CSMASIMULATOR_H
#define CSMASIMULATOR_H
#include <string>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>
#include<iomanip>
#include <algorithm>    // std::count

class CSMASimulator {
private:
    int node_count;
    int packet_sz;
    int retransmissions = -1;
    std::vector<int> retransmission_backoff;
    int simulation_time;

    std::vector<int> ticksToTransmission;
    std::vector<int> currBackoffIndex;
    int currTime = 0;
    int transmissionRemaining = 0;
    int transmittingNode = -1;
    int successfulTransmissionTicks = 0;

    std::vector<int> parseMultipleArgs(char c, std::string currLine);
    void simulateRound();

public:
    CSMASimulator(std::string inputFileName);
    double simulate();
    void printState();
};



#endif //CSMASIMULATOR_H
