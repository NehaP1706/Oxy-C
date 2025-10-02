#include "cshark.h"

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