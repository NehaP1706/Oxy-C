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

#define HEX_DUMP_BYTES 16

static volatile pcap_t *g_handle = NULL;
static volatile int g_capturing = 0;
static unsigned long long g_packet_count = 0;

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
    time_t sec = header->ts.tv_sec;
    long usec = header->ts.tv_usec;

    printf("-----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------\n");
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
                start_capture(chosen);
            } else if (opt == 2) {
                start_capture_with_filter(chosen);
            } else if (opt == 3) {
                printf("Option not implemented in Phase 1.\n\n");
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