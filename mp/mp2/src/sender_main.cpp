/* 
 * File:   sender_main.c
 * Author: 
 *
 * Created on 
 */

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>
#include <cmath>
#include <chrono>

#include <algorithm>
#include <iostream>

#include "sender.hpp"

bool update_cwnd(unsigned int ackno) {
  bool resend = false;
  packet container = packet(ackno);

  if (ackno == dup_ack_no) {
    dup_ack_count++;

    // goto Fast Recovery
    if (dup_ack_count == DUP_ACK_LIMIT) {
      ss_thresh = get_cwnd_size() / 2;
      set_cwnd_size(ss_thresh + 3);
      resend = true;
    }

    // already in Fast Recovery
    if (dup_ack_count > DUP_ACK_LIMIT) {
      increase_cwnd_size();
      resend = true;
    }
  } else {
    if (!cwnd.empty() && *cwnd.top() == container) {
      dup_ack_count = 1;
      dup_ack_no = ackno;
    } else if (cwnd.empty() || *cwnd.top() < container) {
      increase_cwnd_size();
    }
  }

  return resend;
}

void setup_socket(char* hostname, unsigned short int hostUDPport) {
  struct timeval timeout;
  slen = sizeof(si_other);

  if ((s = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == -1)
    diep((char*)"socket()");

  memset((char*)&si_other, 0, sizeof(si_other));
  si_other.sin_family = AF_INET;
  si_other.sin_port = htons(hostUDPport);
  if (inet_aton(hostname, &si_other.sin_addr) < 0)
    diep((char*)"inet_aton()");

  timeout.tv_sec = TIMEOUT;
  timeout.tv_usec = TIMEOUT_USEC;
  if (setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0)
    diep((char*)"setsockopt()");
}

void fillVector(std::vector<packet*> &packets, unsigned long long bytesToSend,
  FILE* file) {
  int seqNum = 0;
  while (bytesToSend > 0) {
    packet* curr = new packet(seqNum);
    ssize_t read_bytes = curr->populate(file, std::min(bytesToSend, (unsigned long long) MAX_PACKET_SIZE));

    if(read_bytes <= 0) {
      delete curr;
      break;
    }

    bytesToSend -= read_bytes;
    packets.push_back(curr);
    seqNum++;
  }

}

void fill_cwnd() {
  packet* snd_packet;
  ssize_t snd_bytes, read_bytes;
  int packets_sent = 0;

  while (cwnd.size() < get_cwnd_size() && remaining_bytes > 0) {
    snd_packet = new packet(seq_no);
    read_bytes = snd_packet->populate(
        fp, std::min(remaining_bytes, (unsigned long long int)MAX_PACKET_SIZE));
    if (read_bytes <= 0) {
      delete snd_packet;
      break;
    }
    remaining_bytes -= read_bytes;
    snd_bytes = sendto(s, snd_packet, sizeof(packet), 0,
                       (struct sockaddr*)&si_other, slen);

    if (snd_bytes < 0)
      diep((char*)"sendto()");

    packets_sent++;
    cwnd.push(snd_packet);
    seq_no++;
  }
#if DEBUG
  printf("Sent %d packets with window size %u \n", packets_sent,
         get_cwnd_size());
  printf("Next packet should have seqno %lu\n", seq_no);
#endif
}

void finish_transfer() {
  packet *fin_packet, recv_packet;
  int i;

  fin_packet = new packet(seq_no);
  fin_packet->set_type(PACKET_TYPE_FIN);
  for (i = 0; i < 3; i++) {
    sendto(s, fin_packet, sizeof(packet), 0, (struct sockaddr*)&si_other, slen);
    if (recv(s, &recv_packet, sizeof(packet), 0) >= 0) {
      break;
    }
  }

  delete fin_packet;
}

void reliablyTransfer(char* hostname, unsigned short int hostUDPport,
                      char* filename, unsigned long long int bytesToTransfer) {
  ssize_t bytes;
  packet *snd_packet, recv_packet;
  int ack_packets;

  setup_socket(hostname, hostUDPport);

  // Open the file
  FILE* file = fopen(filename, "rb");
  if (file == NULL)
    diep((char*)"fopen");

  /* Determine how many bytes to transfer */
  remaining_bytes = bytesToTransfer;
  total_packets = (bytesToTransfer + MAX_PACKET_SIZE - 1) / MAX_PACKET_SIZE;
  packets_acked = 0;

  std::vector<packet*> packets;
  fillVector(packets, bytesToTransfer, file);
  printf("Created %lu packets in vector", packets.size());

  //setup congestion window
  double long cw = 1;
  double SST = 64;
  uint32_t dupAck = 0;
  uint64_t cw_base = 0;
  uint64_t last_ack = 0;
  uint8_t state = SS;
  uint64_t max_sent = 0;
  std::queue<int> specialResends;

  //get the time
  auto sent = std::chrono::high_resolution_clock::now();

  while(cw_base < packets.size()) {
    //make any transmissions that are necessary
    for(uint64_t i = max_sent; i < cw_base + cw && i < packets.size(); i++) {
      sendto(s, packets[i], sizeof(packet), 0, (struct sockaddr*)&si_other, slen);
      std::cout << "Sent packet " << i << std::endl;
      max_sent = i;
    }

    //special resends
    while(!specialResends.empty()) {
      int si = specialResends.front();
      specialResends.pop();
      sendto(s, packets[si], sizeof(packet), 0, (struct sockaddr*)&si_other, slen);
    }



    //wait for replies
    packet incomingPkt;
    bool timeout = false;
    int bytes = recv(s, &incomingPkt, sizeof(packet), 0);
    if(bytes < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
      printf("Timeout ocurred");
      timeout = true;
    }

    auto curr = std::chrono::high_resolution_clock::now();
    auto ms_curr = std::chrono::duration_cast<std::chrono::milliseconds>(curr.time_since_epoch()).count();
    auto ms_sent = std::chrono::duration_cast<std::chrono::milliseconds>(sent.time_since_epoch()).count();

    if((ms_curr - ms_sent) > (TIMEOUT_USEC / 1000)) timeout = true;

    if (!timeout && incomingPkt.has_type(PACKET_TYPE_FIN)) {
      break;
    }

    if(!timeout) {
      std::cout << "received packet " << incomingPkt.seqno << "/" << packets.size() << std::endl;
    }

    bool newAck = incomingPkt.seqno > last_ack;
    bool dup = incomingPkt.seqno == last_ack;

    last_ack = last_ack > incomingPkt.seqno ? last_ack : incomingPkt.seqno;
    cw_base = last_ack;


    //implement the state machine
    switch(state) {
      case SS : {
        if(timeout) {
          SST = cw / 2;
          cw = 1;
          dupAck = 0;
          std::cout << "timed out" << std::endl;
          specialResends.push(cw_base);
          max_sent = cw_base;
          //reset timer
          sent = std::chrono::high_resolution_clock::now();
        }
        else if(dup) {
          dupAck++;
          printf("dupacks: %d\n", dupAck);
        }
        //new ack
        else if(newAck) {
          cw += incomingPkt.seqno - last_ack;
          cw_base = incomingPkt.seqno;
          last_ack = incomingPkt.seqno;
          dupAck = 0;
          sent = std::chrono::high_resolution_clock::now();
        }

        //state changes
        if(dupAck == 3) {
          SST = cw / 2;
          cw = SST + 3;
          //retry cw_base
          specialResends.push(cw_base);
          //transfer new packet if allowed. //done auto
          state = FR;
        }
        else if(cw >= SST) state = CA;
        break;
      }
      case CA: {
        if(timeout) {
          SST = cw / 2;
          cw = 1;
          dupAck = 0;
          state = SS;
          specialResends.push(cw_base);
          max_sent = cw_base;
          //reset timer
          sent = std::chrono::high_resolution_clock::now();
        }
        else if(dup) {
          dupAck++;
          printf("dupacks: %d\n", dupAck);
        }
        else if(newAck) {
          int ackedPkts = incomingPkt.seqno - last_ack;
          for(int i = 0; i < ackedPkts; i++) cw = cw + 1.0 / std::floor(cw);
          dupAck = 0;
          last_ack = incomingPkt.seqno;
          cw_base = incomingPkt.seqno;
          sent = std::chrono::high_resolution_clock::now();
          //transmit based on cw - done auto
        }
        if(dupAck == 3) {
          SST = cw/2;
          cw = SST + 3;
          //retransmit CW_base
          specialResends.push(cw_base);
          //transmit new - done auto
          state = FR;
        }
        break;
      }
      case FR: {
        if(timeout) {
          SST = cw / 2;
          cw = 1;
          dupAck = 0;
          specialResends.push(cw_base);
          max_sent = cw_base;
          state = SS;
          sent = std::chrono::high_resolution_clock::now(); //reset timer
        }
        else if(dup) cw++;
        else if(newAck) {
          cw = SST;
          dupAck = 0;
          last_ack = incomingPkt.seqno;
          cw_base = incomingPkt.seqno;
          state = CA;

          sent = std::chrono::high_resolution_clock::now();
        }
      }

      //if congestion window shrank, then we drop the max sent value to the window
      if(cw_base + cw  <= max_sent) max_sent = cw_base + cw - 1;

      //Print out stats
      std::cout << "base" << cw_base << " end " << cw + cw_base << std::endl;

    }

  }

  finish_transfer();
  fclose(fp);
  close(s);

  return;









#if DEBUG
  printf("Finished initial filling, start waiting for acks\n\n");
#endif
  while (packets_acked < total_packets || remaining_bytes > 0) {
    bytes = recv(s, &recv_packet, sizeof(packet), 0);
    if (bytes < 0) {
      if (errno == EAGAIN || errno == EWOULDBLOCK) {
#if DEBUG
        printf("Timeout\n");
#endif
        ss_thresh = get_cwnd_size() / 2;
        set_cwnd_size(1);
#if DEBUG
        printf("Retransmitting packet %lu\n", cwnd.top()->seqno);
#endif
        sendto(s, cwnd.top(), sizeof(packet), 0, (struct sockaddr*)&si_other,
               slen);
        continue;
      }
      diep((char*)"recv");
    }

    if (recv_packet.has_type(PACKET_TYPE_FIN)) {
      break;
    }

    ack_packets = 0;
    while (!cwnd.empty() && *cwnd.top() < recv_packet) {
      snd_packet = cwnd.top();
      ack_packets++;
      cwnd.pop();
      delete snd_packet;
      packets_acked++;
    }

#if DEBUG
    printf("Popped %d packets after receiving ack for %lu\n", ack_packets,
           recv_packet.seqno);
    if (cwnd.empty()) {
      printf("Window is empty\n");
    } else {
      printf("Next expected packet is %lu\n", cwnd.top()->seqno);
    }
#endif

    if (update_cwnd(recv_packet.seqno)) {
#if DEBUG
      printf("Retransmitting packet %lu\n", cwnd.top()->seqno);
#endif
      sendto(s, cwnd.top(), sizeof(packet), 0, (struct sockaddr*)&si_other,
             slen);
    }

    fill_cwnd();
#if DEBUG
    printf("Packet progress (%d/%d) with inflight packets %ld\n\n",
           packets_acked, total_packets, cwnd.size());
#endif
  }

  finish_transfer();
  fclose(fp);
  close(s);
  return;
}

/*
 * 
 */
int main(int argc, char** argv) {

  unsigned short int udpPort;
  unsigned long long int numBytes;

  if (argc != 5) {
    fprintf(stderr,
            "usage: %s receiver_hostname receiver_port filename_to_xfer "
            "bytes_to_xfer\n\n",
            argv[0]);
    exit(1);
  }
  udpPort = (unsigned short int)atoi(argv[2]);
  numBytes = atoll(argv[4]);

  reliablyTransfer(argv[1], udpPort, argv[3], numBytes);

  return (EXIT_SUCCESS);
}
