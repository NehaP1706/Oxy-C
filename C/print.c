#include "cshark.h"

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
