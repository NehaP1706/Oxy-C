#include "cshark.h"

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
    //packet_handler(NULL, hdr, pkt);   // full layered decode
    //print_full_hexdump(pkt, hdr->caplen);  // raw hex dump
    layer_by_layer(pkt, hdr->caplen); // layer-by-layer analysis
}