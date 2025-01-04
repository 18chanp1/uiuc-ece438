#ifndef MP2_SHARED_HPP
#define MP2_SHARED_HPP

#include <stdio.h>

// Define the maximum data size, in bytes, that a packet can carry
// Tweak this value to optimize performance
#define MAX_PACKET_SIZE 1000
#define UDP_MAX 1472
#define DEBUG 0
/**
 * diep prints an error message and exits the program
 */
void diep(char* s) {
  perror(s);
  exit(1);
}

#define PACKET_TYPE_DATA 1 << 1
#define PACKET_TYPE_ACK 1 << 2
#define PACKET_TYPE_FIN 1 << 3

class packet {
 public:
  unsigned long seqno;
  unsigned int data_sz;
  char data[MAX_PACKET_SIZE];
  unsigned int type;

  packet() {
    seqno = 0;
    data_sz = 0;
    type = 0;
    memset(data, 0, MAX_PACKET_SIZE);
  }

  packet(unsigned long seqno) : seqno(seqno) {
    data_sz = 0;
    type = 0;
    memset(data, 0, MAX_PACKET_SIZE);
  }

  /**
   * populate reads the next packet from the file and populates the packet struct respectively
   * 
   * @param fp the file to read from
   * @return the number of bytes read. Error if negative
   * */
  int populate(FILE* fp, size_t size) {
    int err;
    size_t actual;

    err = feof(fp);
    if (err)
      goto populate_error;

    actual = fread(this->data, sizeof(char), size, fp);
    this->data_sz = actual;
    this->set_type(PACKET_TYPE_DATA);
    return actual;

  populate_error:
    return err;
  }

  void copy(packet* p) {
    this->seqno = p->seqno;
    this->data_sz = p->data_sz;
    this->type = p->type;
    memcpy(this->data, p->data, p->data_sz);
  }

  bool operator==(const packet& rhs) { return seqno == rhs.seqno; }

  bool operator!=(const packet& rhs) { return !(*this == rhs); }

  bool operator<(const packet& rhs) { return seqno < rhs.seqno; }

  bool operator>(const packet& rhs) { return seqno > rhs.seqno; }

  bool operator<=(const packet& rhs) { return seqno <= rhs.seqno; }

  bool operator>=(const packet& rhs) { return seqno >= rhs.seqno; }

  void clear_type() { this->type = 0; }

  void set_type(unsigned int type) { this->type |= type; }

  bool has_type(unsigned int type) { return this->type & type; }

  ~packet() {}
};

struct PacketComparator {
  bool operator()(packet* lhs, packet* rhs) const { return *lhs > *rhs; }
};

#endif  // MP2_SHARED_HPP
