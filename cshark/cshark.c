//////////////////////////////// LLM Generated Code Begins //////////////////////////////////////

#include "cshark.h"

volatile pcap_t *g_handle = NULL;
volatile int g_capturing = 0;
unsigned long long g_packet_count = 0;
stored_packet_t *g_packets = NULL;
int g_stored_count = 0;

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

//////////////////////////////// LLM Generated Code Ends //////////////////////////////////////
