//////////////////////////////// LLM Generated Code Begins //////////////////////////////////////

#include "cshark.h"

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

//////////////////////////////// LLM Generated Code Ends //////////////////////////////////////
