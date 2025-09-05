// server.c
// All comments are written by Neha Prabhu, analyzed to a intermediate level.
// Usage: ./server <port> [--chat] [loss_rate]

#include "sham.h"
#include "utils.h"
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <openssl/evp.h>

#define BACKLOG 1
#define RECV_BUF (SHAM_MTU+sizeof(struct sham_header))
#define DEFAULT_WIN 8192

static double g_loss = 0.0;

// Setting SYN Number to a random number
uint32_t rand32() 
{ 
    return (uint32_t) (rand() << 16 ^ rand()); 
}

int main(int argc, char **argv) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <port> [--chat] [loss_rate]\n", argv[0]);
        return 1;
    }

    srand(time(NULL));
    setenv("RUDP_ROLE","server",1);
    // Appears as server -> server_log.txt to the log_event function
    // Appears as client -> client_log.txt to the log_event function

    int port = atoi(argv[1]);
    int chat_mode = 0;

    if (argc >=3 && strcmp(argv[2], "--chat")==0) 
    {
        chat_mode = 1;
    }

    if (!chat_mode && argc >=4) 
    {
        g_loss = atof(argv[3]);
    }

    if (chat_mode && argc >=4) 
    {
        g_loss = atof(argv[3]);
    }

    // Defines a socket with characteristics as: IPv4, Datagram and UDP
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock < 0) 
    { 
        perror("socket"); 
        return 1; 
    }

    // Stores the Socket address of the IPv4 family
    struct sockaddr_in saddr; 
    memset(&saddr,0,sizeof(saddr));
    saddr.sin_family = AF_INET; 

    // Helps for big-endian to little-endian conversion
    saddr.sin_port = htons(port); 

    // Allows any IP address of the machine, not restricted to localhost 
    saddr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sock, (struct sockaddr*)&saddr, sizeof(saddr))<0) 
    { 
        perror("bind"); 
        return 1; 
    }

    printf("Server listening on port %d\n", port);

    // Stores the client address for packet recieving
    struct sockaddr_in cliaddr; 
    socklen_t cli_len = sizeof(cliaddr);

    // RECV_BUF = header + payload size
    uint8_t buf[RECV_BUF];

    // Make sequence numbers across the session consistent
    uint32_t server_seq = 0;    // server's current sequence number 
    uint32_t client_isn = 0;    // client's initial sequence number

    while (1) {
        // Packet retrieval: Socket, buffer to store the packet, sender's address struct
        // r : bytes received
        ssize_t r = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&cliaddr, &cli_len);

        if (r <= 0) 
        {
            // Try again
            continue;
        }

        struct sham_header *hdr = (struct sham_header*)buf;

        // Check client packet to figure out whether it is a connection request
        if (ntohs(hdr->flags) & SHAM_SYN) 
        {
            log_event("RCV SYN SEQ=%u", ntohl(hdr->seq_num));
            
            // Create a struct to send a response of SYN+ACK in response
            struct sham_packet pkt; 
            memset(&pkt,0,sizeof(pkt));
            uint32_t srv_seq = rand32() & 0x7fffffff;

            // record client's ISN and set server_seq to srv_seq so seq is consistent across session
            client_isn = ntohl(hdr->seq_num);
            server_seq = srv_seq;

            // seq_num is randomly generated as discussed in class
            // ack_num is set to client_seq_num+1 to denote successful transmission
            // set both SYN+ACK flags as part of the 3-way handshake
            pkt.hdr.seq_num = htonl(srv_seq);
            pkt.hdr.ack_num = htonl(client_isn + 1);
            pkt.hdr.flags = htons(SHAM_SYN | SHAM_ACK);
            pkt.hdr.window_size = htons(DEFAULT_WIN);

            // Send the packet to the client address
            sendto(sock, &pkt, sizeof(struct sham_header), 0, (struct sockaddr*)&cliaddr, cli_len);
            log_event("SND SYN-ACK SEQ=%u ACK=%u", srv_seq, ntohl(pkt.hdr.ack_num));

            // wait for final ACK
            ssize_t r2 = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&cliaddr, &cli_len);
            
            if (r2 <= 0) 
            { 
                continue; 
            }

            struct sham_header *hdr2 = (struct sham_header*)buf;

            // Check if we received an ACKnowledgement and the ack_num matched the seq_num+1, denoting successful two way transmission
            if ((ntohs(hdr2->flags) & SHAM_ACK) && ntohl(hdr2->ack_num) == srv_seq+1) {
                log_event("RCV ACK FOR SYN");
                // 3-way handshake complete
                break;
            }
        }
    }

    // After handshake: either chat or file-transfer
    if (chat_mode) {
        // Allows to keep both connections valid: terminal input and socket
        fd_set readfds;
        char line[2048];

        while (1) {
            // Initialize checking the terminal and socket
            // Add stdin to the set of file descriptors to the set
            // Add the socket file descriptor to the set
            FD_ZERO(&readfds);
            FD_SET(0,&readfds);
            FD_SET(sock,&readfds);

            // Block the program until at least one of them are ready
            int nf = sock+1;
            select(nf, &readfds, NULL, NULL, NULL);

            // Terminal is ready to be read
            if (FD_ISSET(0,&readfds)) {
                if (!fgets(line,sizeof(line),stdin)) 
                {
                    break;
                }

                // Truncate the new line character to reduce confusion
                int n = strlen(line);
                if (line[n-1] == '\0')
                {
                    line[n-1]='\0';
                    n--;
                }

                // 1. A request for graceful closure of the connection? From the server side.
                if (strcmp(line, "/quit")==0) {
                    // initiate 4-way Termination handshake 
                    struct sham_packet pkt; 
                    memset(&pkt,0,sizeof(pkt));

                    pkt.hdr.seq_num = htonl(server_seq);
                    pkt.hdr.flags = htons(SHAM_FIN);

                    sendto(sock, &pkt, sizeof(struct sham_header), 0, (struct sockaddr*)&cliaddr, cli_len);
                    log_event("SND FIN SEQ=%u", server_seq);

                    server_seq += 1;
                } else {
                    // Send data as a chat application: packet setup
                    struct sham_packet pkt; 
                    memset(&pkt,0,sizeof(pkt));

                    pkt.hdr.seq_num = htonl(server_seq);

                    size_t L = strlen(line);
                    if (L > SHAM_MTU) L = SHAM_MTU; 
                    memcpy(pkt.data, line, L);

                    sendto(sock, &pkt, sizeof(struct sham_header)+L, 0, (struct sockaddr*)&cliaddr, cli_len);
                    log_event("SND DATA SEQ=%u LEN=%zu", server_seq, L);

                    server_seq += (uint32_t)L;
                }
            }

            // The socket connection received a packet
            if (FD_ISSET(sock,&readfds)) {
                ssize_t r = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&cliaddr, &cli_len);
                
                if (r<=0) 
                {
                    continue;
                }

                struct sham_header *rh = (struct sham_header*)buf;

                // 1. A request for graceful closure of the connection? From the client side.
                // The 4-way termination handshake is followed
                if (ntohs(rh->flags) & SHAM_FIN) {
                    log_event("RCV FIN SEQ=%u", ntohl(rh->seq_num));

                    // 2. send ACK for FIN from server side
                    struct sham_packet ack; 
                    memset(&ack,0,sizeof(ack));
                    ack.hdr.ack_num = htonl(ntohl(rh->seq_num)+1);
                    ack.hdr.flags = htons(SHAM_ACK);

                    sendto(sock, &ack, sizeof(struct sham_header), 0, (struct sockaddr*)&cliaddr, cli_len);
                    log_event("SND ACK FOR FIN");

                    // 3.Server FIN sent
                    struct sham_packet fin; memset(&fin,0,sizeof(fin));
                    fin.hdr.seq_num = htonl(server_seq);
                    fin.hdr.flags = htons(SHAM_FIN);

                    sendto(sock, &fin, sizeof(struct sham_header), 0, (struct sockaddr*)&cliaddr, cli_len);
                    log_event("SND FIN SEQ=%u", server_seq);

                    server_seq += 1;

                    // 4. Final ACK for connection closure
                    recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&cliaddr, &cli_len);
                    log_event("RCV ACK=%u", ntohl(((struct sham_header*)buf)->ack_num));
                    
                    break;
                } else {
                    // bytes received - header size = payload size, presumably
                    size_t dlen = r - sizeof(struct sham_header);

                    if (dlen>0) {
                        // Print the text onto stdout fd
                        fwrite(buf+sizeof(struct sham_header),1,dlen,stdout);
                        fflush(stdout);

                        log_event("RCV DATA SEQ=%u LEN=%zu", ntohl(((struct sham_header*)buf)->seq_num), dlen);

                        // ACK for the retrieval of the text
                        struct sham_packet ack; 
                        memset(&ack,0,sizeof(ack));
                        ack.hdr.flags = htons(SHAM_ACK);
                        ack.hdr.ack_num = htonl(ntohl(((struct sham_header*)buf)->seq_num) + dlen);
                        ack.hdr.window_size = htons(DEFAULT_WIN);

                        sendto(sock,&ack,sizeof(struct sham_header),0,(struct sockaddr*)&cliaddr,cli_len);
                        log_event("SND ACK=%u WIN=%d", ntohl(ack.hdr.ack_num), DEFAULT_WIN);
                    }
                }
            }
        }
        close(sock);
        return 0;
    }

    // File-transfer mode:
    // open file to write the data to
    char outname[256] = "received_file.bin";
    FILE *out = fopen(outname, "wb");
    if (!out) 
    { 
        perror("fopen"); 
        return 1; 
    }

    // next byte expected
    uint32_t expected = client_isn + 1; 

    // Handling packets coming in out of order: always an issue with UDP
    #define MAX_BUF_PKTS 10240

    struct buffered 
    { 
        uint32_t seq; 
        size_t len; 
        uint8_t data[SHAM_MTU]; 
    } 
    
    *bufs = calloc(MAX_BUF_PKTS, sizeof(*bufs));

    if (!bufs) { perror("calloc"); fclose(out); close(sock); return 1; }

    while (1) {
        // Receive packets from the client
        ssize_t r = recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&cliaddr, &cli_len);

        if (r <= 0) 
        {
            continue;
        }
        // Simulate drop
        if (should_drop(g_loss)) {
            struct sham_header *rh = (struct sham_header*)buf;
            log_event("DROP DATA SEQ=%u", ntohl(rh->seq_num));
            continue;
        }

        struct sham_header *rh = (struct sham_header*)buf;
        uint16_t flags = ntohs(rh->flags);

        // In case the packet is a request for a graceful conection closure
        // Do the 4-way termination handshake 
        if (flags & SHAM_FIN) {
            log_event("RCV FIN SEQ=%u", ntohl(rh->seq_num));

            struct sham_packet ack; memset(&ack,0,sizeof(ack));
            ack.hdr.ack_num = htonl(ntohl(rh->seq_num)+1);
            ack.hdr.flags = htons(SHAM_ACK);

            sendto(sock, &ack, sizeof(struct sham_header), 0, (struct sockaddr*)&cliaddr, cli_len);
            log_event("SND ACK FOR FIN");

            struct sham_packet fin; memset(&fin,0,sizeof(fin));
            fin.hdr.seq_num = htonl(server_seq);
            fin.hdr.flags = htons(SHAM_FIN);

            sendto(sock, &fin, sizeof(struct sham_header), 0, (struct sockaddr*)&cliaddr, cli_len);
            log_event("SND FIN SEQ=%u", server_seq);

            server_seq += 1;

            recvfrom(sock, buf, sizeof(buf), 0, (struct sockaddr*)&cliaddr, &cli_len);
            log_event("RCV ACK=%u", ntohl(((struct sham_header*)buf)->ack_num));

            break;
        }

        // dlen = payload_length = total_bytes_received - header_size
        size_t dlen = (size_t)r - sizeof(struct sham_header);

        uint32_t seq = ntohl(rh->seq_num);
        log_event("RCV DATA SEQ=%u LEN=%zu", seq, dlen);

        // Seems to be in order.
        if (seq == expected) {
            fwrite(buf + sizeof(struct sham_header), 1, dlen, out);
            expected += dlen;

            // check buffered struct for changes
            int changed = 1;

            while (changed) {
                changed = 0;

                for (int i=0;i<MAX_BUF_PKTS;i++) if (bufs[i].seq==expected) {
                    fwrite(bufs[i].data,1,bufs[i].len,out);
                    expected += bufs[i].len;
                    bufs[i].seq = 0; 
                    changed = 1;
                }
            }
        // Some packets went missing in between
        } else if (seq > expected) {
            int placed = 0;

            // Save the ones we already received
            for (int i=0;i<MAX_BUF_PKTS;i++) if (bufs[i].seq==0) 
            { 
                bufs[i].seq = seq; 
                bufs[i].len = dlen; 
                memcpy(bufs[i].data, buf+sizeof(struct sham_header), dlen); 
                placed=1; 
                break; 
            }

            if (!placed) {
                // buffer full; drop (or could send selective NACK in extended implementation)
            }

        } else {
            // seq < expected: duplicate or retransmit of already written data; ignore but still ACK
        }

        // send cumulative ACK for one cycle of retrieved data from the user
        // calculate available window size = buffer capacity - buffered data
        size_t buffered_bytes = 0;
        for (int i=0;i<MAX_BUF_PKTS;i++) {
            if (bufs[i].seq != 0) {
                buffered_bytes += bufs[i].len;
            }
        }

        size_t avail = (MAX_BUF_PKTS * SHAM_MTU) - buffered_bytes;
        
        if (avail > UINT16_MAX) 
        {
            avail = UINT16_MAX;  // clamp to protocol field size
        }

        // send cumulative ACK for one cycle of retrieved data from the user
        struct sham_packet ack; memset(&ack,0,sizeof(ack));
        ack.hdr.flags = htons(SHAM_ACK);
        ack.hdr.ack_num = htonl(expected);
        ack.hdr.window_size = htons((uint16_t)avail);

        sendto(sock, &ack, sizeof(struct sham_header), 0, (struct sockaddr*)&cliaddr, cli_len);
        log_event("SND ACK=%u WIN=%zu", expected, avail);
    }

    // Close the file
    fclose(out);
    free(bufs);

    FILE* f = fopen(outname, "rb");
    if (!f)
    {
        perror("fopen");
        exit;
    }

    // Open an MD5 context
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();
    if (!mdctx) { perror("EVP_MD_CTX_new"); return 1; }

    if (EVP_DigestInit_ex(mdctx, EVP_md5(), NULL) != 1) {
        perror("EVP_DigestInit_ex");
        EVP_MD_CTX_free(mdctx);
        return 1;
    }

    // Read the file in chunks
    unsigned char ibuf[4096]; 
    size_t nr;
    while ((nr = fread(ibuf, 1, sizeof(ibuf), f)) > 0) {
        if (EVP_DigestUpdate(mdctx, ibuf, nr) != 1) {
            perror("EVP_DigestUpdate");
            EVP_MD_CTX_free(mdctx);
            return 1;
        }
    }

    // Get the final digest
    unsigned char md5sum[EVP_MAX_MD_SIZE];
    unsigned int mdlen;
    if (EVP_DigestFinal_ex(mdctx, md5sum, &mdlen) != 1) {
        perror("EVP_DigestFinal_ex");
        EVP_MD_CTX_free(mdctx);
        return 1;
    }

    EVP_MD_CTX_free(mdctx);
    fclose(f);

    // Convert to hex
    char hex[EVP_MAX_MD_SIZE*2+1];
    for (unsigned int i = 0; i < mdlen; i++) {
        sprintf(hex + i*2, "%02x", md5sum[i]);
    }
    hex[mdlen*2] = '\0';

    printf("MD5: %s\n", hex);

    // complete
    close(sock);
    return 0;
}
