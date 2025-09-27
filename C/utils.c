#include "cshark.h"

void handle_sigint(int signo) {
    (void)signo;
    if (g_capturing && g_handle) {
        // Break out of pcap_loop
        pcap_breakloop((pcap_t *)g_handle);
    }
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