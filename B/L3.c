#include "cshark.h"

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
