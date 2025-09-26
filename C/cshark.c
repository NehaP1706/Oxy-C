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

#ifndef TH_ECE
#define TH_ECE 0x40
#endif
#ifndef TH_CWR
#define TH_CWR 0x80
#endif

#ifndef IPPROTO_TCP
#define IPPROTO_TCP 6
#endif
#ifndef IPPROTO_UDP  
#define IPPROTO_UDP 17
#endif

static volatile pcap_t *g_handle = NULL;
static volatile int g_capturing = 0;
static unsigned long long g_packet_count = 0;

typedef struct {
    struct pcap_pkthdr header;    // packet metadata
    u_char *data;                 // deep copy of the packet
} stored_packet_t;

static stored_packet_t *g_packets = NULL;
static int g_stored_count = 0;

static inline uint16_t get_u16_be(const u_char *p, int off, int len) {
    if (off + 1 >= len) return 0;
    return (uint16_t)((p[off] << 8) | p[off + 1]);
}
static inline uint32_t get_u32_be(const u_char *p, int off, int len) {
    if (off + 3 >= len) return 0;
    return (uint32_t)((p[off] << 24) | (p[off + 1] << 16) | (p[off + 2] << 8) | p[off + 3]);
}

void handle_sigint(int signo) {
    (void)signo;
    if (g_capturing && g_handle) {
        // Break out of pcap_loop
        pcap_breakloop((pcap_t *)g_handle);
    }
}

void print_hex(const u_char *buf, int len) {
    for (int i = 0; i < len; ++i) {
        printf("%02X ", buf[i]);
    }
}

void print_mac(const u_char *mac) {
    printf("%02X:%02X:%02X:%02X:%02X:%02X", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

void print_full_hexdump(const u_char *data, int len) {
    printf("\n[Full Hex Dump of Packet]\n");
    for (int i = 0; i < len; i += 16) {
        printf("%04X  ", i);  // offset
        // hex section
        for (int j = 0; j < 16 && i + j < len; j++) {
            printf("%02X ", data[i + j]);
        }
        // padding for short lines
        for (int j = (len - i < 16 ? len - i : 16); j < 16; j++) {
            printf("   ");
        }
        printf(" ");
        // ASCII section
        for (int j = 0; j < 16 && i + j < len; j++) {
            unsigned char c = data[i + j];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        printf("\n");
    }
    printf("\n");
}

void print_hex_ascii(const u_char *data, int len, int max_bytes) {
    int to_print = len < max_bytes ? len : max_bytes;

    for (int i = 0; i < to_print; i += 16) {
        int line_len = (to_print - i > 16) ? 16 : (to_print - i);

        // Print hex part
        for (int j = 0; j < line_len; j++) {
            printf("%02X ", data[i + j]);
        }

        // Pad if less than 16 bytes
        for (int j = line_len; j < 16; j++) {
            printf("   ");
        }

        printf(" ");

        // Print ASCII part
        for (int j = 0; j < line_len; j++) {
            unsigned char c = data[i + j];
            if (c >= 32 && c <= 126) {
                printf("%c", c);
            } else {
                printf(".");
            }
        }

        printf("\n");
    }
}

void print_hex_ascii_slice(const u_char *pkt, int plen, int start, int end) {
    if (start >= plen) {
        printf("         [out of packet bounds]\n");
        return;
    }
    if (end >= plen) end = plen - 1;
    int total = end - start + 1;
    int to_print = total > MAX_FIELD_DISPLAY ? MAX_FIELD_DISPLAY : total;

    // Print hex with better formatting
    printf("         ");
    for (int i = 0; i < to_print; i++) {
        printf("%02X", pkt[start + i]);
        if (i + 1 < to_print) printf(" ");
        if ((i + 1) % 16 == 0 && i + 1 < to_print) {
            printf("\n         ");
        }
    }
    if (to_print < total) printf(" ...");
    
    // Add ASCII representation on the same line for short fields, or next line for long ones
    if (to_print <= 8) {
        printf("  (");
        for (int i = 0; i < to_print; i++) {
            unsigned char c = pkt[start + i];
            printf("%c", (c >= 32 && c <= 126) ? c : '.');
        }
        printf(")");
    }
    printf("\n");
    
    // For longer fields, show ASCII on separate lines
    if (to_print > 8) {
        printf("         ");
        for (int i = 0; i < to_print; i++) {
            unsigned char c = pkt[start + i];
            printf(" %c", (c >= 32 && c <= 126) ? c : '.');
            if ((i + 1) % 16 == 0 && i + 1 < to_print) {
                printf("\n         ");
            }
        }
        if (to_print < total) printf(" ...");
        printf("\n");
    }
}

void print_field_bytes(const char *name, const u_char *pkt, int plen, int start, int end, const char *extra) {
    if (start >= plen) {
        printf("     %-30s [OUT OF BOUNDS]\n", name);
        return;
    }
    if (end >= plen) end = plen - 1;

    printf("    %-30s", name);
    if (extra && extra[0]) {
        printf(" %s", extra);
    }
    printf("\n");
    
    printf("    Bytes %d-%d (%d bytes)\n", start, end, end - start + 1);
    print_hex_ascii_slice(pkt, plen, start, end);
    printf("    \n");
}

void print_layer_header(const char *layer_name, const char *description) {
    printf("\n");
    printf("---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    printf("                                                                       %-20s | %-55s \n", layer_name, description);
    printf("---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
}

void layer_by_layer(const u_char *pkt, int plen) {
    if (!pkt || plen <= 0) {
        printf("[layer_by_layer] empty packet\n");
        return;
    }

    char extra[256];

    printf("\n");
    printf("---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    printf("                                                                        PACKET ANALYSIS REPORT                                  \n");
    printf("                                                                        Total Length: %d bytes                                  \n", plen);
    printf("---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

    const int eth_len = 14;
    if (plen < eth_len) {
        printf("  [ERROR] Truncated Ethernet header\n");
        print_field_bytes("Raw Frame", pkt, plen, 0, plen - 1, "");
        return;
    }

    // --- Layer 2: Ethernet ---
    print_layer_header("LAYER 2", "Data Link Layer - Ethernet");
    
    snprintf(extra, sizeof(extra),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             pkt[0], pkt[1], pkt[2], pkt[3], pkt[4], pkt[5]);
    print_field_bytes("Destination MAC", pkt, plen, 0, 5, extra);

    snprintf(extra, sizeof(extra),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             pkt[6], pkt[7], pkt[8], pkt[9], pkt[10], pkt[11]);
    print_field_bytes("Source MAC", pkt, plen, 6, 11, extra);

    uint16_t ethertype = get_u16_be(pkt, 12, plen);
    const char* eth_type_name = (ethertype == ETHERTYPE_IP) ? "IPv4" :
                               (ethertype == ETHERTYPE_IPV6) ? "IPv6" :
                               (ethertype == ETHERTYPE_ARP) ? "ARP" : "Unknown";
    snprintf(extra, sizeof(extra), "0x%04X (%s)", ethertype, eth_type_name);
    print_field_bytes("EtherType", pkt, plen, 12, 13, extra);

    // --- Layer 3: Network Layer ---
    if (ethertype == ETHERTYPE_IP) {
        print_layer_header("LAYER 3", "Network Layer - IPv4");
        
        int ip_off = eth_len;
        if (ip_off + 20 > plen) {
            printf("    [ERROR] IPv4 Header truncated\n");
            return;
        }

        uint8_t ver_ihl = pkt[ip_off];
        uint8_t version = ver_ihl >> 4;
        uint8_t ihl = (ver_ihl & 0x0F) * 4;
        uint8_t tos = pkt[ip_off + 1];
        uint16_t totlen = get_u16_be(pkt, ip_off + 2, plen);
        uint16_t id = get_u16_be(pkt, ip_off + 4, plen);
        uint16_t frag = get_u16_be(pkt, ip_off + 6, plen);
        uint8_t ttl = pkt[ip_off + 8];
        uint8_t proto = pkt[ip_off + 9];
        uint16_t checksum = get_u16_be(pkt, ip_off + 10, plen);
        uint32_t srcip = get_u32_be(pkt, ip_off + 12, plen);
        uint32_t dstip = get_u32_be(pkt, ip_off + 16, plen);

        snprintf(extra, sizeof(extra),
                 "Version: %d | Header Length: %d bytes | ToS: 0x%02X | Total Length: %u bytes", 
                 version, ihl, tos, totlen);
        print_field_bytes("Version & Header Info", pkt, plen, ip_off, ip_off + 3, extra);

        bool dont_frag = (frag & 0x4000) != 0;
        bool more_frags = (frag & 0x2000) != 0;
        uint16_t frag_offset = (frag & 0x1FFF) * 8;
        snprintf(extra, sizeof(extra),
                 "ID: 0x%04X | DF:%s MF:%s | Fragment Offset: %u", 
                 id, dont_frag ? "1" : "0", more_frags ? "1" : "0", frag_offset);
        print_field_bytes("Identification & Flags", pkt, plen, ip_off + 4, ip_off + 7, extra);

        const char* proto_name = (proto == IPPROTO_TCP) ? "TCP" :
                                (proto == IPPROTO_UDP) ? "UDP" :
                                (proto == IPPROTO_ICMP) ? "ICMP" : "Other";
        snprintf(extra, sizeof(extra),
                 "TTL: %u hops | Protocol: %u (%s) | Checksum: 0x%04X", 
                 ttl, proto, proto_name, checksum);
        print_field_bytes("TTL & Protocol", pkt, plen, ip_off + 8, ip_off + 11, extra);

        struct in_addr a;
        a.s_addr = htonl(srcip);
        char ssrc[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &a, ssrc, sizeof(ssrc));
        print_field_bytes("Source IP Address", pkt, plen, ip_off + 12, ip_off + 15, ssrc);

        a.s_addr = htonl(dstip);
        char sdst[INET_ADDRSTRLEN]; inet_ntop(AF_INET, &a, sdst, sizeof(sdst));
        print_field_bytes("Destination IP Address", pkt, plen, ip_off + 16, ip_off + 19, sdst);

        if (ihl > 20) {
            print_field_bytes("IP Options", pkt, plen, ip_off + 20, ip_off + ihl - 1, "Variable length options");
        }

        // --- Layer 4: Transport Layer ---
        int l4_off = ip_off + ihl;
        if (proto == IPPROTO_TCP && l4_off + 20 <= plen) {
            print_layer_header("LAYER 4", "Transport Layer - TCP");
            
            uint16_t sport = get_u16_be(pkt, l4_off, plen);
            uint16_t dport = get_u16_be(pkt, l4_off + 2, plen);
            uint32_t seq = get_u32_be(pkt, l4_off + 4, plen);
            uint32_t ack = get_u32_be(pkt, l4_off + 8, plen);
            uint8_t data_off_raw = pkt[l4_off + 12];
            uint8_t tcp_hdr_len = (data_off_raw >> 4) * 4;
            uint8_t flags = pkt[l4_off + 13];
            uint16_t win = get_u16_be(pkt, l4_off + 14, plen);
            uint16_t chksum = get_u16_be(pkt, l4_off + 16, plen);

            snprintf(extra, sizeof(extra), "Source Port: %u | Destination Port: %u", sport, dport);
            print_field_bytes("Port Numbers", pkt, plen, l4_off, l4_off + 3, extra);

            snprintf(extra, sizeof(extra), "Sequence Number: %u", seq);
            print_field_bytes("Sequence Number", pkt, plen, l4_off + 4, l4_off + 7, extra);

            snprintf(extra, sizeof(extra), "Acknowledgment Number: %u", ack);
            print_field_bytes("Acknowledgment Number", pkt, plen, l4_off + 8, l4_off + 11, extra);

            char flag_str[64] = {0};
            if (flags & 0x01) strcat(flag_str, "FIN ");
            if (flags & 0x02) strcat(flag_str, "SYN ");
            if (flags & 0x04) strcat(flag_str, "RST ");
            if (flags & 0x08) strcat(flag_str, "PSH ");
            if (flags & 0x10) strcat(flag_str, "ACK ");
            if (flags & 0x20) strcat(flag_str, "URG ");
            if (strlen(flag_str) == 0) strcpy(flag_str, "None");
            
            snprintf(extra, sizeof(extra), "Header Length: %u bytes | Flags: %s│ Window: %u", 
                     tcp_hdr_len, flag_str, win);
            print_field_bytes("Control Info", pkt, plen, l4_off + 12, l4_off + 15, extra);

            snprintf(extra, sizeof(extra), "Checksum: 0x%04X | Urgent Pointer: %u", 
                     chksum, get_u16_be(pkt, l4_off + 18, plen));
            print_field_bytes("Checksum & Urgent", pkt, plen, l4_off + 16, l4_off + 19, extra);

            if (tcp_hdr_len > 20) {
                print_field_bytes("TCP Options", pkt, plen, l4_off + 20, l4_off + tcp_hdr_len - 1, "Variable length TCP options");
            }

            int payload_off = l4_off + tcp_hdr_len;
            if (payload_off < plen) {
                print_layer_header("LAYER 7", "Application Layer - TCP Payload");
                int payload_len = plen - payload_off;
                if (payload_len > 0) {
                    printf("Payload (%d bytes):\n", payload_len);
                    print_full_hexdump(pkt + payload_off, payload_len);
                    printf("\n");
                }
            }

        } else if (proto == IPPROTO_UDP && l4_off + 8 <= plen) {
            print_layer_header("LAYER 4", "Transport Layer - UDP");
            
            uint16_t sport = get_u16_be(pkt, l4_off, plen);
            uint16_t dport = get_u16_be(pkt, l4_off + 2, plen);
            uint16_t ulen = get_u16_be(pkt, l4_off + 4, plen);
            uint16_t chksum = get_u16_be(pkt, l4_off + 6, plen);

            snprintf(extra, sizeof(extra), "Source: %u | Destination: %u | Length: %u bytes | Checksum: 0x%04X", 
                     sport, dport, ulen, chksum);
            print_field_bytes("UDP Header", pkt, plen, l4_off, l4_off + 7, extra);

            int payload_off = l4_off + 8;
            if (payload_off < plen) {
                print_layer_header("LAYER 7", "Application Layer - UDP Payload");
                int payload_len = plen - payload_off;
                if (payload_len > 0) {
                    printf("Payload (%d bytes):\n", payload_len);
                    print_hex_ascii(pkt + payload_off, payload_len, INT_MAX);
                    printf("\n");
                }
            }
        }

    } 
    // --- IPv6 ---
    else if (ethertype == ETHERTYPE_IPV6) {
        print_layer_header("LAYER 3", "Network Layer - IPv6");
        
        int ip6_off = eth_len;
        if (ip6_off + 40 > plen) { 
            printf("    [ERROR] IPv6 Header truncated\n"); 
            return; 
        }

        uint32_t vtf = ntohl(*(uint32_t *)(pkt + ip6_off));
        int version = (vtf >> 28) & 0xF;
        int traffic_class = (vtf >> 20) & 0xFF;
        int flow_label = vtf & 0xFFFFF;
        
        snprintf(extra, sizeof(extra), "Version: %d | Traffic Class: 0x%02X | Flow Label: 0x%05X", 
                 version, traffic_class, flow_label);
        print_field_bytes("Version & Traffic Info", pkt, plen, ip6_off, ip6_off + 3, extra);

        uint16_t payload_len = get_u16_be(pkt, ip6_off + 4, plen);
        uint8_t next_hdr = pkt[ip6_off + 6];
        uint8_t hop = pkt[ip6_off + 7];
        
        const char* next_hdr_name = (next_hdr == IPPROTO_TCP) ? "TCP" :
                                   (next_hdr == IPPROTO_UDP) ? "UDP" :
                                   (next_hdr == 58) ? "ICMPv6" : "Other";
        
        snprintf(extra, sizeof(extra), "Payload: %u bytes | Next Header: %u (%s) | Hop Limit: %u", 
                 payload_len, next_hdr, next_hdr_name, hop);
        print_field_bytes("Payload & Next Header", pkt, plen, ip6_off + 4, ip6_off + 7, extra);

        char srcbuf[INET6_ADDRSTRLEN], dstbuf[INET6_ADDRSTRLEN];
        inet_ntop(AF_INET6, pkt + ip6_off + 8, srcbuf, sizeof(srcbuf));
        inet_ntop(AF_INET6, pkt + ip6_off + 24, dstbuf, sizeof(dstbuf));
        
        print_field_bytes("Source IPv6 Address", pkt, plen, ip6_off + 8, ip6_off + 23, srcbuf);
        print_field_bytes("Destination IPv6 Address", pkt, plen, ip6_off + 24, ip6_off + 39, dstbuf);

        // --- Layer 4: Transport Layer for IPv6 ---
        int l4_off = ip6_off + 40;
        if (next_hdr == IPPROTO_TCP && l4_off + 20 <= plen) {
            print_layer_header("LAYER 4", "Transport Layer - TCP over IPv6");
            
            uint16_t sport = get_u16_be(pkt, l4_off, plen);
            uint16_t dport = get_u16_be(pkt, l4_off + 2, plen);
            uint32_t seq = get_u32_be(pkt, l4_off + 4, plen);
            uint32_t ack = get_u32_be(pkt, l4_off + 8, plen);
            uint8_t data_off_raw = pkt[l4_off + 12];
            uint8_t tcp_hdr_len = (data_off_raw >> 4) * 4;
            uint8_t flags = pkt[l4_off + 13];
            uint16_t win = get_u16_be(pkt, l4_off + 14, plen);
            uint16_t chksum = get_u16_be(pkt, l4_off + 16, plen);

            snprintf(extra, sizeof(extra), "Source Port: %u | Destination Port: %u", sport, dport);
            print_field_bytes("Port Numbers", pkt, plen, l4_off, l4_off + 3, extra);

            snprintf(extra, sizeof(extra), "Sequence Number: %u", seq);
            print_field_bytes("Sequence Number", pkt, plen, l4_off + 4, l4_off + 7, extra);

            snprintf(extra, sizeof(extra), "Acknowledgment Number: %u", ack);
            print_field_bytes("Acknowledgment Number", pkt, plen, l4_off + 8, l4_off + 11, extra);

            char flag_str[64] = {0};
            if (flags & 0x01) strcat(flag_str, "FIN ");
            if (flags & 0x02) strcat(flag_str, "SYN ");
            if (flags & 0x04) strcat(flag_str, "RST ");
            if (flags & 0x08) strcat(flag_str, "PSH ");
            if (flags & 0x10) strcat(flag_str, "ACK ");
            if (flags & 0x20) strcat(flag_str, "URG ");
            if (strlen(flag_str) == 0) strcpy(flag_str, "None");
            
            snprintf(extra, sizeof(extra), "Header Length: %u bytes | Flags: %s | Window: %u", 
                     tcp_hdr_len, flag_str, win);
            print_field_bytes("Control Info", pkt, plen, l4_off + 12, l4_off + 15, extra);

            snprintf(extra, sizeof(extra), "Checksum: 0x%04X | Urgent Pointer: %u", 
                     chksum, get_u16_be(pkt, l4_off + 18, plen));
            print_field_bytes("Checksum & Urgent", pkt, plen, l4_off + 16, l4_off + 19, extra);

            if (tcp_hdr_len > 20) {
                print_field_bytes("TCP Options", pkt, plen, l4_off + 20, l4_off + tcp_hdr_len - 1, "Variable length TCP options");
            }

            int payload_off = l4_off + tcp_hdr_len;
            if (payload_off < plen) {
                print_layer_header("LAYER 7", "Application Layer - TCP Payload (IPv6)");
                int payload_len = plen - payload_off;
                if (payload_len > 0) {
                    printf("Payload (%d bytes):\n", payload_len);
                    print_full_hexdump(pkt + payload_off, payload_len);
                    printf("\n");
                }
            }

        } else if (next_hdr == IPPROTO_UDP && l4_off + 8 <= plen) {
            print_layer_header("LAYER 4", "Transport Layer - UDP over IPv6");
            
            uint16_t sport = get_u16_be(pkt, l4_off, plen);
            uint16_t dport = get_u16_be(pkt, l4_off + 2, plen);
            uint16_t ulen = get_u16_be(pkt, l4_off + 4, plen);
            uint16_t chksum = get_u16_be(pkt, l4_off + 6, plen);

            snprintf(extra, sizeof(extra), "Source: %u | Destination: %u | Length: %u bytes | Checksum: 0x%04X", 
                     sport, dport, ulen, chksum);
            print_field_bytes("UDP Header", pkt, plen, l4_off, l4_off + 7, extra);

            int payload_off = l4_off + 8;
            if (payload_off < plen) {
                print_layer_header("LAYER 7", "Application Layer - UDP Payload (IPv6)");
                int payload_len = plen - payload_off;
                if (payload_len > 0) {
                    printf("Payload (%d bytes):\n", payload_len);
                    print_full_hexdump(pkt + payload_off, payload_len);
                    printf("\n");
                }
            }
        }
    }
    // --- ARP ---
    else if (ethertype == ETHERTYPE_ARP) {
        print_layer_header("LAYER 3", "Network Layer - ARP (Address Resolution Protocol)");
        
        int arp_off = eth_len;
        if (arp_off + 28 > plen) { 
            printf("    [ERROR] ARP packet truncated\n"); 
            return; 
        }
        
        uint16_t htype = get_u16_be(pkt, arp_off, plen);
        uint16_t ptype = get_u16_be(pkt, arp_off + 2, plen);
        //uint8_t hlen = pkt[arp_off + 4];
        //uint8_t plen_sz = pkt[arp_off + 5];
        uint16_t op = get_u16_be(pkt, arp_off + 6, plen);
        
        const char* op_name = (op == 1) ? "Request" : (op == 2) ? "Reply" : "Unknown";
        
        snprintf(extra, sizeof(extra), "HW Type: 0x%04X | Protocol: 0x%04X | Operation: %u (%s)", 
                 htype, ptype, op, op_name);
        print_field_bytes("ARP Header", pkt, plen, arp_off, arp_off + 7, extra);
    }
    // --- Unknown EtherType ---
    else {
        print_layer_header("LAYER 3", "Unknown Protocol");
        snprintf(extra, sizeof(extra), "EtherType 0x%04X not recognized", ethertype);
        print_field_bytes("Unknown Protocol Data", pkt, plen, eth_len, plen - 1, extra);
    }

    printf("\n");
    printf("---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    printf("                                                                                   END OF ANALYSIS                                      \n");
    printf("---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");

    printf("\n");
}

void decode_arp(const u_char *packet) {
    struct ether_arp *arp = (struct ether_arp *)packet;

    // Basic operation
    const char *op_str = "Unknown";
    if (ntohs(arp->ea_hdr.ar_op) == ARPOP_REQUEST) op_str = "Request";
    else if (ntohs(arp->ea_hdr.ar_op) == ARPOP_REPLY) op_str = "Reply";

    // Convert IPs to string
    char sip[INET_ADDRSTRLEN], tip[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, arp->arp_spa, sip, sizeof(sip));
    inet_ntop(AF_INET, arp->arp_tpa, tip, sizeof(tip));

    // L3: ARP header info
    printf("L3 (ARP): Sender IP: %s | Target IP: %s | Sender MAC: %02X:%02X:%02X:%02X:%02X:%02X | Target MAC: %02X:%02X:%02X:%02X:%02X:%02X |\nHW type: 0x%04X | Proto type: 0x%04X | HW len: %d | Proto len: %d\n",
           sip, tip, arp->arp_sha[0], arp->arp_sha[1], arp->arp_sha[2],
           arp->arp_sha[3], arp->arp_sha[4], arp->arp_sha[5], 
           arp->arp_tha[0], arp->arp_tha[1], arp->arp_tha[2],
           arp->arp_tha[3], arp->arp_tha[4], arp->arp_tha[5],
           ntohs(arp->ea_hdr.ar_hrd),
           ntohs(arp->ea_hdr.ar_pro),
           arp->ea_hdr.ar_hln,
           arp->ea_hdr.ar_pln);
    printf("Operation: %s (%u)\n", op_str, ntohs(arp->ea_hdr.ar_op));

    // L4: Sender/Target addresses
    printf("L4: Not Applicable for ARP.\n");           

    // L7: ARP has no payload
    printf("L7 (Payload): None (ARP does not carry higher-layer data)\n");
}

void decode_tcp(const struct tcphdr *tcp, int payload_len, const u_char *payload) {
    uint16_t src_port = ntohs(tcp->th_sport);
    uint16_t dst_port = ntohs(tcp->th_dport);

    // Identify common ports
    const char *src_proto = "Unknown";
    const char *dst_proto = "Unknown";

    if (src_port == 80) src_proto = "HTTP";
    else if (src_port == 443) src_proto = "HTTPS";
    else if (src_port == 53) src_proto = "DNS";

    if (dst_port == 80) dst_proto = "HTTP"; 
    else if (dst_port == 443) dst_proto = "HTTPS"; 
    else if (dst_port == 53) dst_proto = "DNS"; 

    printf("L4 (TCP): Src Port: %u (%s) | Dst Port: %u (%s)\n", src_port, src_proto, dst_port, dst_proto);
    printf("Seq: %u | Ack: %u\n", ntohl(tcp->th_seq), ntohl(tcp->th_ack));

    // Decode flags
    printf("Flags: [");
    if (tcp->th_flags & TH_SYN) printf("SYN,");
    if (tcp->th_flags & TH_ACK) printf("ACK,");
    if (tcp->th_flags & TH_FIN) printf("FIN,");
    if (tcp->th_flags & TH_RST) printf("RST,");
    if (tcp->th_flags & TH_PUSH) printf("PSH,");
    if (tcp->th_flags & TH_URG) printf("URG,");
    printf("]\n");

    printf("Window: %u | Checksum: 0x%04X | Header Length: %d bytes\n",
           ntohs(tcp->th_win), ntohs(tcp->th_sum), tcp->th_off * 4);

    if (strcmp(dst_proto, "Unknown") != 0 || strcmp(src_proto, "Unknown") != 0) {
        const char* proto_to_print = strcmp(dst_proto, "Unknown") != 0 ? dst_proto : src_proto;
        int port_to_print = strcmp(dst_proto, "Unknown") != 0 ? dst_port : src_port;

        printf("L7 (Payload): Identified as %s on port %u - %d bytes\n",
               proto_to_print, port_to_print, payload_len);
        printf("Data (first %d bytes):\n", payload_len < 64 ? payload_len : 64);
        print_hex_ascii(payload, payload_len, 64);
    }
    else
    {
        printf("L7 (Payload): %d bytes (unknown TCP service)\n", payload_len);
        printf("Data (first %d bytes):\n", payload_len < 64 ? payload_len : 64);
        print_hex_ascii(payload, payload_len, 64);
    }
}

void decode_udp(const struct udphdr *udp, int payload_len, const u_char *payload) {
    uint16_t sport = ntohs(udp->uh_sport);
    uint16_t dport = ntohs(udp->uh_dport);

    printf("L4 (UDP): Src Port: %u | Dst Port: %u | Length: %u | Checksum: 0x%04X\n",
           sport, dport,
           ntohs(udp->uh_ulen), ntohs(udp->uh_sum));

    if (payload_len > 0) {
        if (sport == 53 || dport == 53) {
            printf("L7 (Payload): Identified as DNS - %d bytes\n", payload_len);
        }
        else if (sport == 443 || dport == 443) {
            printf("L7 (Payload): Identified as HTTPS/QUIC - %d bytes\n", payload_len);
        }
        else {
            printf("L7 (Payload): %d bytes (unknown UDP service)\n", payload_len);
        }

        printf("Data (first %d bytes):\n", payload_len < 64 ? payload_len : 64);
        print_hex_ascii(payload, payload_len, 64);
    } else {
        printf("L7 (Payload): None (0 bytes)\n");
    }
}

void decode_ipv6(const u_char *packet, int len) {
    if (len < (int)sizeof(struct ip6_hdr)) {
        printf("Truncated IPv6 header.\n");
        return;
    }

    const struct ip6_hdr *ip6 = (const struct ip6_hdr *)packet;
    char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN];
    inet_ntop(AF_INET6, &ip6->ip6_src, src, sizeof(src));
    inet_ntop(AF_INET6, &ip6->ip6_dst, dst, sizeof(dst));

    const char *proto_name;
    switch(ip6->ip6_nxt) {
        case IPPROTO_TCP: proto_name = "TCP"; break;
        case IPPROTO_UDP: proto_name = "UDP"; break;
        default: proto_name = "Unknown"; break;
    }

    printf("L3 (IPv6): Src IP: %s | Dst IP: %s | Next Header: %s (%d) | Hop Limit: %d\n",
       src, dst, proto_name, ip6->ip6_nxt, ip6->ip6_hops);

    printf("Traffic Class: %d | Flow Label: 0x%05X | Payload Length: %d\n",
           (ntohl(ip6->ip6_flow) >> 20) & 0xFF,
           ntohl(ip6->ip6_flow) & 0xFFFFF,
           ntohs(ip6->ip6_plen));

    const u_char *payload = packet + sizeof(struct ip6_hdr);
    int payload_len = ntohs(ip6->ip6_plen);

    if (ip6->ip6_nxt == IPPROTO_TCP && payload_len >= (int)sizeof(struct tcphdr)) {
        const struct tcphdr *tcp = (const struct tcphdr *)payload;
        int tcp_hdr_len = tcp->th_off * 4;
        decode_tcp(tcp, payload_len - tcp_hdr_len, payload + tcp_hdr_len);
    } else if (ip6->ip6_nxt == IPPROTO_UDP && payload_len >= (int)sizeof(struct udphdr)) {
        const struct udphdr *udp = (const struct udphdr *)payload;
        decode_udp(udp, payload_len - sizeof(struct udphdr), payload + sizeof(struct udphdr));
    }
}

void decode_ipv4(const u_char *packet, int len) {
    if (len < (int)sizeof(struct ip)) {
        printf("Truncated IPv4 header.\n");
        return;
    }

    const struct ip *iph = (const struct ip *)packet;
    char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &iph->ip_src, src, sizeof(src));
    inet_ntop(AF_INET, &iph->ip_dst, dst, sizeof(dst));

    int iphdrlen = iph->ip_hl * 4;
    int totlen   = ntohs(iph->ip_len);

    // Flags + fragment offset are combined in ip_off
    int flags = ntohs(iph->ip_off);
    int df = (flags & IP_DF) ? 1 : 0;
    int mf = (flags & IP_MF) ? 1 : 0;

    const char *proto_str = "Unknown";
    if (iph->ip_p == IPPROTO_TCP) proto_str = "TCP";
    else if (iph->ip_p == IPPROTO_UDP) proto_str = "UDP";

    printf("L3 (IPv4): Src IP: %s | Dst IP: %s | Proto: %s (%d) | TTL: %d\n",
           src, dst, proto_str, iph->ip_p, iph->ip_ttl);
    printf("Packet ID: %u | Total Length: %d | Header Length: %d bytes | Flags:",
           ntohs(iph->ip_id), totlen, iphdrlen);
    if (df) printf(" DF");
    if (mf) printf(" MF");
    if (!df && !mf) printf(" None");
    printf("\n");

    const u_char *payload = packet + iphdrlen;
    int payload_len = totlen - iphdrlen;

    if (iph->ip_p == IPPROTO_TCP && payload_len >= (int)sizeof(struct tcphdr)) {
        const struct tcphdr *tcp = (const struct tcphdr *)payload;
        int tcp_hdr_len = tcp->th_off * 4;
        decode_tcp(tcp, payload_len - tcp_hdr_len, payload + tcp_hdr_len);
    } else if (iph->ip_p == IPPROTO_UDP && payload_len >= (int)sizeof(struct udphdr)) {
        const struct udphdr *udp = (const struct udphdr *)payload;
        decode_udp(udp, payload_len - sizeof(struct udphdr), payload + sizeof(struct udphdr));
    }
}

void packet_handler(u_char *args, const struct pcap_pkthdr *header, const u_char *packet) {
    (void)args;
    g_packet_count++;

    if (g_stored_count < MAX_PACKETS) {
        g_packets[g_stored_count].header = *header; // shallow copy
        g_packets[g_stored_count].data = malloc(header->caplen);
        if (g_packets[g_stored_count].data) {
            memcpy(g_packets[g_stored_count].data, packet, header->caplen);
        }
        g_stored_count++;
    }

    time_t sec = header->ts.tv_sec;
    long usec = header->ts.tv_usec;

    printf("---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
    printf("Packet #%llu | Timestamp: %ld.%06ld | Length: %u bytes\n",
           g_packet_count, (long)sec, usec, header->caplen);

    const struct ether_header *eth = (const struct ether_header *)packet;
    printf("L2 (Ethernet): Dst MAC: ");
    print_mac(eth->ether_dhost);
    printf(" | Src MAC: ");
    print_mac(eth->ether_shost);
    printf(" |\n");

    uint16_t etype = ntohs(eth->ether_type);
    if (etype == ETHERTYPE_IP) {
        printf("EtherType: IPv4 (0x%04X)\n", etype);
        decode_ipv4(packet + sizeof(struct ether_header),
                    header->caplen - sizeof(struct ether_header));
    } else if (etype == ETHERTYPE_IPV6) {
        printf("EtherType: IPv6 (0x%04X)\n", etype);
        decode_ipv6(packet + sizeof(struct ether_header),
                    header->caplen - sizeof(struct ether_header));
    } else if (etype == ETHERTYPE_ARP) {
        printf("EtherType: ARP (0x%04X)\n", etype);
        decode_arp(packet + sizeof(struct ether_header));
    } else {
        printf("EtherType: 0x%04X (unknown)\n", etype);
    }
}

int select_device(pcap_if_t *alldevs, char *chosen, size_t chosen_len) {
    pcap_if_t *d;
    int index = 0;
    printf("[C-Shark] Searching for available interfaces... Found!\n\n");

    for (d = alldevs; d != NULL; d = d->next) {
        index++;
        printf("%2d. %s", index, d->name);
        if (d->description)
            printf(" — %s", d->description);
        printf("\n");
    }
    
    printf("\nSelect an interface to sniff (1-%d): ", index);

    char line[128];
    if (fgets(line, sizeof(line), stdin) == NULL) return -1; // EOF
    int choice = atoi(line);
    if (choice < 1) return -2;
    int i = 0;
    
    for (d = alldevs; d != NULL; d = d->next) {
        i++;
        if (i == choice) {
            strncpy(chosen, d->name, chosen_len - 1);
            chosen[chosen_len - 1] = '\0';
            return 0;
        }
    }

    return -3; // invalid
}

void start_capture(const char *dev) {
    char errbuf[PCAP_ERRBUF_SIZE];
    bpf_u_int32 net, mask;
    if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {
        net = 0; 
        mask = 0;
    }

    g_packet_count = 0;

    pcap_t *handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Error opening device %s: %s\n", dev, errbuf);
        return;
    }

    g_handle = handle;
    g_capturing = 1;

    printf("\n[C-Shark] Started capturing on '%s' — press Ctrl+C to stop and return to menu.\n", dev);

    // Start the capture loop. -1 means capture until pcap_breakloop is called.
    if (pcap_loop(handle, -1, packet_handler, NULL) < 0) {
        // pcap_loop returns -1 on an error other than breakloop; -2 indicates breakloop (pcap_breakloop) occurred
        if (pcap_geterr(handle)) {
            const char *err = pcap_geterr(handle);
            if (err && strlen(err) > 0) {
                fprintf(stderr, "pcap_loop error: %s\n", err);
            }
        }
    }

    // Clean up
    pcap_close(handle);
    g_handle = NULL;
    g_capturing = 0;
    printf("\n[C-Shark] Capture stopped. Returned to main menu.\n\n");
}

void start_capture_with_filter(const char *dev) {
    char errbuf[PCAP_ERRBUF_SIZE];
    bpf_u_int32 net, mask;
    if (pcap_lookupnet(dev, &net, &mask, errbuf) == -1) {
        net = 0;
        mask = 0;
    }

    pcap_t *handle = pcap_open_live(dev, BUFSIZ, 1, 1000, errbuf);
    if (handle == NULL) {
        fprintf(stderr, "Error opening device %s: %s\n", dev, errbuf);
        return;
    }

    // Prompt user for filter choice
    printf("\nChoose a filter:\n");
    printf("1. HTTP\n");
    printf("2. HTTPS\n");
    printf("3. DNS\n");
    printf("4. ARP\n");
    printf("5. TCP\n");
    printf("6. UDP\n");
    printf("Enter choice: ");

    char line[32];
    if (fgets(line, sizeof(line), stdin) == NULL) {
        pcap_close(handle);
        return;
    }
    int choice = atoi(line);

    const char *filter_exp = NULL;
    switch (choice) {
        case 1: filter_exp = "tcp port 80"; break;     // HTTP
        case 2: filter_exp = "tcp port 443"; break;    // HTTPS
        case 3: filter_exp = "udp port 53"; break;     // DNS
        case 4: filter_exp = "arp"; break;             // ARP
        case 5: filter_exp = "tcp"; break;             // TCP
        case 6: filter_exp = "udp"; break;             // UDP
        default: filter_exp = ""; break;               // No filter
    }

    // Compile and apply the filter
    struct bpf_program fp;
    if (pcap_compile(handle, &fp, filter_exp, 0, net) == -1) {
        fprintf(stderr, "Error compiling filter %s: %s\n", filter_exp, pcap_geterr(handle));
        pcap_close(handle);
        return;
    }
    if (pcap_setfilter(handle, &fp) == -1) {
        fprintf(stderr, "Error setting filter %s: %s\n", filter_exp, pcap_geterr(handle));
        pcap_freecode(&fp);
        pcap_close(handle);
        return;
    }
    pcap_freecode(&fp);

    printf("\n[C-Shark] Started capturing with filter '%s' — press Ctrl+C to stop.\n", filter_exp);
    g_handle = handle;
    g_capturing = 1;

    if (pcap_loop(handle, -1, packet_handler, NULL) < 0) {
        const char *err = pcap_geterr(handle);
        if (err && strlen(err) > 0)
            fprintf(stderr, "pcap_loop error: %s\n", err);
    }

    pcap_close(handle);
    g_handle = NULL;
    g_capturing = 0;
    printf("\n[C-Shark] Filtered capture stopped. Returned to main menu.\n\n");
}

void free_stored_packets() {
    if (!g_packets) {
        g_stored_count = 0;
        return; // nothing to free
    }

    for (int i = 0; i < g_stored_count; i++) {
        free(g_packets[i].data);
    }
    free(g_packets);
    g_packets = NULL;
    g_stored_count = 0;
}

void inspect_last_session() {
    if (g_stored_count == 0) {
        printf("\n[C-Shark] No packets stored. Run a capture first.\n\n");
        return;
    }

    printf("\n[C-Shark] Packets from last session (%d stored):\n", g_stored_count);
    for (int i = 0; i < g_stored_count; i++) {
        struct pcap_pkthdr *h = &g_packets[i].header;
        const u_char *pkt = g_packets[i].data;
        const struct ether_header *eth = (const struct ether_header *)pkt;
        uint16_t etype = ntohs(eth->ether_type);

        // Build a short summary for L3/L4
        char summary[128] = "Unknown";
        if (etype == ETHERTYPE_IP) {
            const struct ip *iph = (const struct ip *)(pkt + sizeof(struct ether_header));
            char src[INET_ADDRSTRLEN], dst[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &iph->ip_src, src, sizeof(src));
            inet_ntop(AF_INET, &iph->ip_dst, dst, sizeof(dst));
            if (iph->ip_p == IPPROTO_TCP) {
                const struct tcphdr *tcp = (const struct tcphdr *)((const u_char *)iph + iph->ip_hl * 4);
                snprintf(summary, sizeof(summary), "IPv4 TCP %s:%u -> %s:%u",
                         src, ntohs(tcp->th_sport), dst, ntohs(tcp->th_dport));
            } else if (iph->ip_p == IPPROTO_UDP) {
                const struct udphdr *udp = (const struct udphdr *)((const u_char *)iph + iph->ip_hl * 4);
                snprintf(summary, sizeof(summary), "IPv4 UDP %s:%u -> %s:%u",
                         src, ntohs(udp->uh_sport), dst, ntohs(udp->uh_dport));
            } else {
                snprintf(summary, sizeof(summary), "IPv4 %s -> %s Proto=%d", src, dst, iph->ip_p);
            }
        } else if (etype == ETHERTYPE_IPV6) {
            const struct ip6_hdr *ip6 = (const struct ip6_hdr *)(pkt + sizeof(struct ether_header));
            char src[INET6_ADDRSTRLEN], dst[INET6_ADDRSTRLEN];
            inet_ntop(AF_INET6, &ip6->ip6_src, src, sizeof(src));
            inet_ntop(AF_INET6, &ip6->ip6_dst, dst, sizeof(dst));
            snprintf(summary, sizeof(summary), "IPv6 %s -> %s NextHdr=%d", src, dst, ip6->ip6_nxt);
        } else if (etype == ETHERTYPE_ARP) {
            snprintf(summary, sizeof(summary), "ARP");
        }

        printf("%d. Packet | Timestamp: %ld.%06ld | Length: %u bytes | %s\n",
               i + 1, (long)h->ts.tv_sec, (long)h->ts.tv_usec, h->caplen, summary);
    }

    printf("\nEnter Packet ID to inspect in detail (1-%d): ", g_stored_count);
    char line[32];
    if (fgets(line, sizeof(line), stdin) == NULL) return;
    int id = atoi(line);

    if (id < 1 || id > g_stored_count) {
        printf("Invalid ID.\n\n");
        return;
    }

    const u_char *pkt = g_packets[id - 1].data;
    struct pcap_pkthdr *hdr = &g_packets[id - 1].header;

    printf("\n[C-Shark] Detailed view of packet %d:\n", id);
    packet_handler(NULL, hdr, pkt);   // full layered decode
    //print_full_hexdump(pkt, hdr->caplen);  // raw hex dump
    layer_by_layer(pkt, hdr->caplen); // layer-by-layer analysis
}

int main(void) {
    signal(SIGINT, handle_sigint);

    while (1) {
        pcap_if_t *alldevs;
        char errbuf[PCAP_ERRBUF_SIZE];

        if (pcap_findalldevs(&alldevs, errbuf) == -1) {
            fprintf(stderr, "Error finding devices: %s\n", errbuf);
            return 1;
        }

        char chosen[256];
        int sel = select_device(alldevs, chosen, sizeof(chosen));
        pcap_freealldevs(alldevs);

        if (sel == -1) {
            // EOF (Ctrl+D) — exit
            printf("\n[C-Shark] EOF detected. Exiting.\n");
            return 0;
        } else if (sel < 0) {
            printf("Invalid selection. Try again.\n\n");
            continue;
        }

        // Main menu
        while (1) {
            printf("[C-Shark] Interface '%s' selected. What's next?\n\n", chosen);
            printf("1. Start Sniffing (All Packets)\n");
            printf("2. Start Sniffing (With Filters)\n");
            printf("3. Inspect Last Session\n");
            printf("4. Exit C-Shark\n");
            printf("\nChoose an option: ");

            char line[32];
            if (fgets(line, sizeof(line), stdin) == NULL) {
                // Ctrl+D or EOF
                printf("\n[C-Shark] EOF detected. Exiting.\n");
                return 0;
            }

            int opt = atoi(line);
            if (opt == 1) {
                free_stored_packets();
                g_packets = calloc(MAX_PACKETS, sizeof(stored_packet_t));
                if (!g_packets) {
                    fprintf(stderr, "Memory allocation failed for packet storage\n");
                    return 1;
                }
                start_capture(chosen);
            } else if (opt == 2) {
                free_stored_packets();
                g_packets = calloc(MAX_PACKETS, sizeof(stored_packet_t));
                if (!g_packets) {
                    fprintf(stderr, "Memory allocation failed for packet storage\n");
                    return 1;
                }
                start_capture_with_filter(chosen);
            } else if (opt == 3) {
                inspect_last_session();
            } else if (opt == 4) {
                printf("Exiting C-Shark. Bye!\n");
                return 0;
            } else {
                printf("Invalid option. Try again.\n\n");
            }
        }
    }

    return 0;
}