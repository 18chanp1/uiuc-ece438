/*
* File:   receiver_main.cpp
 * Author:
 *
 * Created on
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <fstream>
#include <functional>
#include <iostream>
#include <queue>

#include "shared.hpp"

struct sockaddr_in si_me, si_other;
int s;
socklen_t slen;
std::priority_queue<packet*, std::vector<packet*>, PacketComparator>
    packetQueue;

void* get_in_addr(struct sockaddr* sa) {
  if (sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/**
 * reliablyReceive receives a file from the sender and writes it to destinationFile
 */
void reliablyReceive(unsigned short int myUDPport, char* destinationFile) {
  packet *snd_packet, recv_packet, *top, *curr;
  int numBytes, i;
  unsigned long nextSeq = 0;
  FILE* outfile;
  bool terminate = false;

  slen = sizeof(si_other);

  if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    diep((char*)"socket");

  memset((char*)&si_me, 0, sizeof(si_me));
  si_me.sin_family = AF_INET;
  si_me.sin_port = htons(myUDPport);
  si_me.sin_addr.s_addr = htonl(INADDR_ANY);
  if (bind(s, (struct sockaddr*)&si_me, sizeof(si_me)) == -1)
    diep((char*)"bind");
#if DEBUG
  printf("Listening on port %d\n", myUDPport);
#endif
  /* Now receive data and send acknowledgements */

  // we skip the handshake because fuck that

  //setup file for writing
  outfile = fopen(destinationFile, "wb");
  if (outfile == NULL)
    diep((char*)"fopen");

  snd_packet = new packet();
  while (!terminate) {
    numBytes = recvfrom(s, &recv_packet, sizeof(packet), 0,
                        (struct sockaddr*)&si_other, (socklen_t*)&slen);
#if DEBUG
    printf("Receive packet %lu of size (%d)\n", recv_packet.seqno, numBytes);
#endif


    if (recv_packet.has_type(PACKET_TYPE_DATA)) {
      curr = new packet();
      curr->copy(&recv_packet);
      packetQueue.push(curr);
    }

    if (recv_packet.has_type(PACKET_TYPE_FIN)) {
      terminate = true;
    }
    while (!packetQueue.empty() && packetQueue.top()->seqno <= nextSeq) {
      top = packetQueue.top();
      packetQueue.pop();
      if (top->seqno < nextSeq) {
        delete top;
        continue;
      }

#if DEBUG
      printf("Write seqno %lu packet\n", top->seqno);
#endif

      fwrite(top->data, sizeof(char), top->data_sz, outfile);
      //prepare for next packet
      delete top;
      nextSeq++;
    }

#if DEBUG
    printf("Ask for next seq %lu\n\n", nextSeq);
#endif
    snd_packet->seqno = nextSeq;
    snd_packet->set_type(PACKET_TYPE_ACK);
    //send the acknowledgement
    sendto(s, snd_packet, sizeof(packet), 0, (struct sockaddr*)&si_other, slen);
  }

  snd_packet->set_type(PACKET_TYPE_FIN);
  for (i = 0; i < 3; i++) {
    sendto(s, snd_packet, sizeof(packet), 0, (struct sockaddr*)&si_other, slen);
  }

  delete snd_packet;
  fclose(outfile);
  close(s);
#if DEBUG
  printf("%s received.\n", destinationFile);
#endif
  return;
}

/*
 *
 */
int main(int argc, char** argv) {
  unsigned short int udpPort;

  if (argc != 3) {
    fprintf(stderr, "usage: %s UDP_port filename_to_write\n\n", argv[0]);
    exit(1);
  }

  udpPort = (unsigned short int)atoi(argv[1]);

  reliablyReceive(udpPort, argv[2]);
}
