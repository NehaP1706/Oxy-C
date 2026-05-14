#include <stdint.h>

#define SHAM_MTU 1024

// Flags
#define SHAM_SYN 0x1
#define SHAM_ACK 0x2
#define SHAM_FIN 0x4

#pragma pack(push,1)

// Recommended S.H.A.M. Header Structure

struct sham_header {
    uint32_t seq_num; // Sequence Number
    uint32_t ack_num; // Acknowledgment Number
    uint16_t flags; // Control flags (SYN, ACK, FIN)
    uint16_t window_size; // Flow control window size
};

#pragma pack(pop)

// A full packet on the wire
struct sham_packet {
    struct sham_header hdr;
    uint8_t data[SHAM_MTU]; 
};