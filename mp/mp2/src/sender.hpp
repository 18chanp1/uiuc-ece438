#ifndef SENDER_HPP
#define SENDER_HPP

#include <functional>
#include <queue>
#include <vector>

#include "shared.hpp"

// Default Slow Start Threshold
#define DEFAULT_SS_THRESH 64
// Default Congestion Window Size
#define DEFAULT_CWND 1
#define CWND_DECEMAL 1000
// Default Duplicate Acknowledgement Limit
#define DUP_ACK_LIMIT 3
// Default timeout value in seconds
#define TIMEOUT 0
#define TIMEOUT_USEC 500000

#define SS 1
#define CA 2
#define FR 3
#define NIGGER 69

// Socket fields
struct sockaddr_in si_other;
int s;
socklen_t slen;
FILE* fp = NULL;

// Congestion control fields
unsigned int dup_ack_count = 0;
unsigned long dup_ack_no = -1;
unsigned long seq_no = 0;
// Multiple by 1000 to avoid floating point arithmetic,
// Divide by 1000 when using the value
unsigned int cwnd_size = DEFAULT_CWND * CWND_DECEMAL;
unsigned int ss_thresh = DEFAULT_SS_THRESH;
unsigned long long int remaining_bytes = 0;
std::priority_queue<packet*, std::vector<packet*>, PacketComparator> cwnd;

unsigned int get_cwnd_size() {
  return cwnd_size / CWND_DECEMAL;
}

void set_cwnd_size(unsigned int size) {
  cwnd_size = size * CWND_DECEMAL;
}

void increase_cwnd_size() {
  // Fast Recovery mode
  if (dup_ack_count > DUP_ACK_LIMIT) {
    cwnd_size += CWND_DECEMAL;
    return;
  }

  // Slow Start mode
  if (get_cwnd_size() < ss_thresh * CWND_DECEMAL) {
    cwnd_size += CWND_DECEMAL;
  } else {
    // Congestion Avoidance mode
    cwnd_size += CWND_DECEMAL / cwnd_size;
  }
}

// Statistic fields
unsigned int total_packets, packets_acked;

/**
 * update_cwnd updates the congestion window size based on the ackno
 * 
 * When an ackno is received, the congestion window size is updated based on the ackno
 *
 * - Slow Start:
 *  -# On new ack, cwnd.size() + 1
 *  -# Once the cwnd.size() size reaches ss_thresh, goto Congestion Avoidance
 *  -# On dup ack, the duplicate ack count is incremented
 *  -# Once the dup_ack_count reaches DUP_ACK_LIMIT, goto Fast Recovery, transmit the missing packet if allowed
 * 
 * - Congestion Avoidance:
 *  -# On new ack, cwnd.size() + 1 / cwnd.size()
 *  -# On dup ack, the duplicate ack count is incremented
 *  -# Once the dup_ack_count reaches DUP_ACK_LIMIT, goto Fast Recovery, transmit the missing packet if allowed
 *  -# On timeout, ss_thresh is set to cwnd.size() / 2 and cwnd.resize(1)
 * 
 * - Fast Recovery:
 *  -# On new ack, cwnd.resize(ss_thresh)
 *  -# On dup ack, cwnd.size() + 1
 *  -# Transmit the missing packet if allowed
 * 
 * @param ackno the ackno to update the cwnd.size() size
 * @return true if need to retransmit the missing packet
 */
bool update_cwnd(unsigned int ackno);

/**
 * setup_socket sets up the socket for the sender
 * 
 * @param hostname the hostname of the receiver
 * @param hostUDPport the UDP port of the receiver
 */
void setup_socket(char* hostname, unsigned short int hostUDPport);

/** 
 * finish_transfer sends the FIN packet to the receiver to signal the end of the transfer
 */
void finish_transfer();

/**
 * reliablyTransfer transfer the first bytesToTransfer bytes of filename to the receiver at hostname: hostUDPport, even if the network drops or reorders some of your packets.
 * 
 * @param hostname the hostname of the receiver
 * @param hostUDPport the UDP port of the receiver
 * @param filename the name of the file to transfer
 * @param bytesToTransfer the number of bytes to transfer
 */
void reliablyTransfer(char* hostname, unsigned short int hostUDPport,
                      char* filename, unsigned long long int bytesToTransfer);

#endif