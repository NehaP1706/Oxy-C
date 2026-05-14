//////////////////////////////// LLM Generated Code Begins //////////////////////////////////////

#ifndef CSHARK_H
#define CSHARK_H

#include <pcap.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/if_ether.h>
#include <netinet/ip6.h>
#include <netinet/ether.h>
#include <stdbool.h>
#include <limits.h>

#define HEX_DUMP_BYTES 16
#define MAX_PACKETS 10000   // limit of stored packets
#define MAX_FIELD_DISPLAY 256

typedef struct {
    struct pcap_pkthdr header;    // packet metadata
    u_char *data;                 // deep copy of the packet
} stored_packet_t;

extern volatile pcap_t *g_handle;
extern volatile int g_capturing;
extern unsigned long long g_packet_count;

extern stored_packet_t *g_packets;
extern int g_stored_count;

static inline uint16_t get_u16_be(const u_char *p, int off, int len) {
    if (off + 1 >= len) return 0;
    return (uint16_t)((p[off] << 8) | p[off + 1]);
}

static inline uint32_t get_u32_be(const u_char *p, int off, int len) {
    if (off + 3 >= len) return 0;
    return (uint32_t)((p[off] << 24) | (p[off + 1] << 16) | (p[off + 2] << 8) | p[off + 3]);
}

void handle_sigint(int signo);
void free_stored_packets();

void start_capture(const char *dev);
void start_capture_with_filter(const char *dev);

void packet_handler(u_char *args, const struct pcap_pkthdr *header, const u_char *packet);
int select_device(pcap_if_t *alldevs, char *chosen, size_t chosen_len);

void print_hex(const u_char *buf, int len);
void print_mac(const u_char *mac);
void print_full_hexdump(const u_char *data, int len);
void print_hex_ascii(const u_char *data, int len, int max_bytes);
void print_hex_ascii_slice(const u_char *pkt, int plen, int start, int end);
void print_field_bytes(const char *name, const u_char *pkt, int plen, int start, int end, const char *extra);
void print_layer_header(const char *layer_name, const char *description);

void decode_arp(const u_char *packet);
void decode_ipv6(const u_char *packet, int len);
void decode_ipv4(const u_char *packet, int len);

void decode_tcp(const struct tcphdr *tcp, int payload_len, const u_char *payload);
void decode_udp(const struct udphdr *udp, int payload_len, const u_char *payload);

void inspect_last_session();
void layer_by_layer(const u_char *pkt, int plen);

#endif // CSHARK_H
//////////////////////////////// LLM Generated Code Ends //////////////////////////////////////
