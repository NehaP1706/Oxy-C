#include "cshark.h"

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