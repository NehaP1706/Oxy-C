// RUDP Client: file transfer + chat mode
// Usage:
// File transfer: ./client <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]
// Chat: ./client <server_ip> <server_port> --chat [loss_rate]
// All comments are written by Neha Prabhu, analyzed to a intermediate level.

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
#include <sys/time.h>
#include <fcntl.h>

#define RECV_BUF (SHAM_MTU+sizeof(struct sham_header))
#define DEFAULT_WIN 8192
#define MAX_INFLIGHT 10
#define RTO_MS 500

static double g_loss = 0.0;

typedef struct {
    uint32_t seq;
    size_t len;
    uint8_t data[SHAM_MTU];
    int acked;
    struct timeval sent;
} outpkt_t;

outpkt_t inflight[MAX_INFLIGHT];
int in_use[MAX_INFLIGHT];

// Setting SYN Number to a random number
uint32_t rand32() 
{ 
    return (uint32_t)(rand() << 16 ^ rand()); 
}

// Sending a plain data packet
// Configures the data header and payload size explicitly
// Sends only the relevant portion
static void send_packet(int sock, struct sockaddr_in *srv, socklen_t slen, uint32_t seq, const uint8_t *data, size_t len, uint16_t win) {
    struct sham_packet pkt;

    memset(&pkt, 0, sizeof(pkt));
    pkt.hdr.seq_num = htonl(seq);
    pkt.hdr.window_size = htons(win);

    if (len > 0) 
    {
        memcpy(pkt.data, data, len);
    }

    sendto(sock, &pkt, sizeof(struct sham_header)+len, 0,(struct sockaddr*)srv, slen);
    log_event("SND DATA SEQ=%u LEN=%zu WIN=%u", seq, len, win);
}

// Checks the inflight packets and retransmits the ones that have not been acknowledged within a time period
static void retransmit_due(int sock, struct sockaddr_in *srv, socklen_t slen, uint16_t win) {
    // Gets the time of the system to millisecond precision, used for tracking
    struct timeval now;
    gettimeofday(&now, NULL);

    for (int i=0;i<MAX_INFLIGHT;i++) 
    {
        // If the packet is in-flight and has not been acknowledged yet:
        if (in_use[i] && !inflight[i].acked) 
        {
            // Calculates the age of the packet in milliseconds
            long ms = (now.tv_sec - inflight[i].sent.tv_sec)*1000 + (now.tv_usec - inflight[i].sent.tv_usec)/1000;
            
            // If it is older than the designated Time out period
            if (ms >= RTO_MS) {

                // Retransmit the packet
                send_packet(sock,srv,slen,inflight[i].seq,inflight[i].data,inflight[i].len,win);
                gettimeofday(&inflight[i].sent,NULL);
                log_event("TIMEOUT SEQ=%u", inflight[i].seq);
                log_event("RETX DATA SEQ=%u LEN=%zu", inflight[i].seq, inflight[i].len);
            }
        }
    }
}

int main(int argc, char **argv) {

    if (argc < 3) {
        fprintf(stderr,"Usage: %s <server_ip> <server_port> <input_file> <output_file_name> [loss_rate]\n",argv[0]);
        fprintf(stderr,"   or: %s <server_ip> <server_port> --chat [loss_rate]\n",argv[0]);
        return 1;
    }

    srand(time(NULL));
    setenv("RUDP_ROLE","client",1);
    // Appears as server -> server_log.txt to the log_event function
    // Appears as client -> client_log.txt to the log_event function

    char *server_ip = argv[1];
    int port = atoi(argv[2]);

    int chat_mode = 0;

    char *infile=NULL,*outname=NULL;

    if (strcmp(argv[3],"--chat")==0) {
        chat_mode=1;
        if (argc>=5) g_loss = atof(argv[4]);
    } else {
        // File transfer mode
        infile=argv[3];
        outname=argv[4];
        if (argc>=6) g_loss = atof(argv[5]);
    }

    // Socket configuration : returns an fd
    // Defines a socket with characteristics as: IPv4, Datagram and UDP
    int sock = socket(AF_INET, SOCK_DGRAM, 0);

    if (sock<0) 
    { 
        perror("socket"); 
        return 1; 
    }

    // Stores the Socket address of the IPv4 family
    struct sockaddr_in srv; socklen_t slen=sizeof(srv);
    memset(&srv,0,sizeof(srv));
    srv.sin_family = AF_INET;

    // Helps for big-endian to little-endian conversion 
    srv.sin_port = htons(port);

    // Converts the ip address from presentation to network represetation for machine understanding
    inet_pton(AF_INET, server_ip, &srv.sin_addr);

    uint8_t buf[RECV_BUF];
    uint16_t remote_win = DEFAULT_WIN;  

    // Make connection request
    // Commence 3-way Handshake for connection
    // 1. Send a request with SYN fag set
    uint32_t cli_seq = rand32() & 0x7fffffff;

    struct sham_packet syn; memset(&syn,0,sizeof(syn));
    syn.hdr.seq_num = htonl(cli_seq);
    syn.hdr.flags = htons(SHAM_SYN);

    sendto(sock,&syn,sizeof(struct sham_header),0,(struct sockaddr*)&srv,slen);
    log_event("SND SYN SEQ=%u",cli_seq);

    // 2. Receive SYN-ACK from the server
    ssize_t r = recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr*)&srv,&slen);

    if (r<=0) 
    { 
        perror("recv SYN-ACK"); 
        return 1; 
    }

    struct sham_header *h = (struct sham_header*)buf;

    // Check whether both SYN and ACK flags are set or not
    if ((ntohs(h->flags)&(SHAM_SYN|SHAM_ACK))==(SHAM_SYN|SHAM_ACK)) {
        remote_win = ntohs(h->window_size);

        log_event("RCV SYN-ACK SEQ=%u ACK=%u WIN=%u",ntohl(h->seq_num),ntohl(h->ack_num),remote_win);

        struct sham_packet ack; memset(&ack,0,sizeof(ack));
        ack.hdr.seq_num = htonl(cli_seq+1);
        ack.hdr.ack_num = htonl(ntohl(h->seq_num)+1);
        ack.hdr.flags = htons(SHAM_ACK);

        // 3. Send final ACK to confirm connection establishment
        sendto(sock,&ack,sizeof(struct sham_header),0,(struct sockaddr*)&srv,slen);
        log_event("RCV ACK FOR SYN");
    }

    if (chat_mode) {
        // Set of file descriptors to be monitoring, for readiness to be read
        fd_set readfds;
        char line[2048];

        while (1) {
            // Add both the terminal and socket side fds to the stack
            FD_ZERO(&readfds);
            FD_SET(0,&readfds);
            FD_SET(sock,&readfds);
            
            // Block the program until at least one of them are ready
            int nf = sock+1;
            select(nf,&readfds,NULL,NULL,NULL);

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
                    struct sham_packet fin; 
                    memset(&fin,0,sizeof(fin));

                    fin.hdr.seq_num=htonl(7777);
                    fin.hdr.flags=htons(SHAM_FIN);

                    sendto(sock,&fin,sizeof(struct sham_header),0,(struct sockaddr*)&srv,slen);
                    log_event("SND FIN SEQ=7777");
                } else {
                    // Plain data received, send to the server

                    size_t L=strlen(line);
                    send_packet(sock,&srv,slen,cli_seq+10,(uint8_t*)line,L,remote_win);
                }
            }

            // The socket connection received a packet
            if (FD_ISSET(sock,&readfds)) {
                ssize_t r2=recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr*)&srv,&slen);

                if (r2<=0) 
                {
                    continue;
                }

                struct sham_header *rh=(struct sham_header*)buf;

                // 1. A request for graceful closure of the connection? From the client side.
                // The 4-way termination handshake is followed
                if (ntohs(rh->flags)&SHAM_FIN) {
                    log_event("RCV FIN SEQ=%u",ntohl(rh->seq_num));

                    // 2. send ACK for FIN from client side
                    struct sham_packet ack; memset(&ack,0,sizeof(ack));
                    ack.hdr.ack_num=htonl(ntohl(rh->seq_num)+1);
                    ack.hdr.flags=htons(SHAM_ACK);
                    
                    sendto(sock,&ack,sizeof(struct sham_header),0,(struct sockaddr*)&srv,slen);
                    log_event("SND ACK FOR FIN");

                    break;
                } else {
                    // Plain data packet received from the server, print to the terminal
                    size_t dlen=r2-sizeof(struct sham_header);

                    if (dlen>0) {
                        fwrite(buf+sizeof(struct sham_header),1,dlen,stdout);
                        fflush(stdout);

                        log_event("RCV DATA SEQ=%u LEN=%zu",ntohl(rh->seq_num),dlen);

                        // ACKnowledge the sent data packet
                        struct sham_packet ack; memset(&ack,0,sizeof(ack));
                        ack.hdr.flags=htons(SHAM_ACK);
                        ack.hdr.ack_num=htonl(ntohl(rh->seq_num)+dlen);
                        ack.hdr.window_size=htons(remote_win);

                        sendto(sock,&ack,sizeof(struct sham_header),0,(struct sockaddr*)&srv,slen);
                        log_event("SND ACK=%u WIN=%u",ntohl(ack.hdr.ack_num),remote_win);
                    }
                }
            }
        }

        close(sock);
        return 0;
    }

    // File transfer mode
    // Open the file to send
    FILE *in=fopen(infile,"rb");
    if (!in) 
    { 
        perror("fopen"); 
        return 1; 
    }

    uint32_t base=1; 
    uint32_t nextseq=1;
    int done=0;

    while (!done) {
        // check how many files have already been sent but not acknowledged yet
        size_t in_flight_bytes = 0;
        for (int i=0;i<MAX_INFLIGHT;i++) 
        {
            if (in_use[i]) 
            {
                in_flight_bytes += inflight[i].len;
            }
        }

        // Respect the advertized window size, don't send more than the receiver can handle
        for (int i=0;i<MAX_INFLIGHT;i++) 
        {
            // Find an unused slot in the inflight array
            if (!in_use[i]) {
                // If the file contents have ended, break.
                if (feof(in)) 
                {
                    break;
                }

                // If the receiver has no space to accept the incoming bytes being sent
                if (in_flight_bytes >= remote_win) 
                {
                    break;
                }

                // Reads SHAM_MTU bytes from the files
                size_t n=fread(inflight[i].data,1,SHAM_MTU,in);

                if (n<=0) 
                { 
                    done=1; 
                    break; 
                }

                // If sending this chunk would just overflow the window, break out.
                if (in_flight_bytes + n > remote_win) {
                    fseek(in, -(long)n, SEEK_CUR);
                    break;
                }

                // Set the inflight details and send the chunk to the server
                inflight[i].seq=nextseq;
                inflight[i].len=n;
                inflight[i].acked=0;

                gettimeofday(&inflight[i].sent,NULL);

                in_use[i]=1;

                send_packet(sock,&srv,slen,inflight[i].seq,inflight[i].data,n,remote_win);
                nextseq += n;
                in_flight_bytes += n;
            }
            
            // Wait for a response from the socket for about 200ms
            struct timeval tv={0,200*1000};

            // Set up the fds to be monitored
            // Add the terminal and the socket
            fd_set fds; 
            FD_ZERO(&fds); 
            FD_SET(sock,&fds);

            int rv=select(sock+1,&fds,NULL,NULL,&tv);

            // If the socket receives a packet
            if (rv>0 && FD_ISSET(sock,&fds)) {
                ssize_t rr=recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr*)&srv,&slen);

                if (rr>0) {
                    struct sham_header *ah=(struct sham_header*)buf;


                    // Check if its an ACKnowledgement for the sent chunks
                    if (ntohs(ah->flags)&SHAM_ACK) {
                        uint32_t ack=ntohl(ah->ack_num);
                        remote_win = ntohs(ah->window_size);

                        log_event("RCV ACK=%u WIN=%u",ack,remote_win);
                        
                        // Goes through the sent chunks and marks them as acknowledged if the ack_num exceeds their length
                        for (int i=0;i<MAX_INFLIGHT;i++) if (in_use[i]) {
                            if (inflight[i].seq+inflight[i].len<=ack) {
                                in_use[i]=0;
                                inflight[i].acked=1;
                            }
                        }
                        base=ack;
                    }
                }
            }
            // Retransmission of the unmarked inflight files.
            retransmit_due(sock,&srv,slen,remote_win);
        }
    }

    // Send a request to close the connection gracefully
    // Commence the 4-way termination Handshake

    // 1. Send FIN
    struct sham_packet fin; 
    memset(&fin,0,sizeof(fin));
    fin.hdr.seq_num=htonl(nextseq);
    fin.hdr.flags=htons(SHAM_FIN);

    sendto(sock,&fin,sizeof(struct sham_header),0,(struct sockaddr*)&srv,slen);
    log_event("SND FIN SEQ=%u",nextseq);

    // 2 & 3. Receive the SYN-ACK from the server
    // 4. Send ACK to proceed with closing the connection
    recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr*)&srv,&slen);
    log_event("RCV ACK=%u",ntohl(((struct sham_header*)buf)->ack_num));

    recvfrom(sock,buf,sizeof(buf),0,(struct sockaddr*)&srv,&slen);
    log_event("RCV FIN SEQ=%u",ntohl(((struct sham_header*)buf)->seq_num));

    struct sham_packet ack2; memset(&ack2,0,sizeof(ack2));
    ack2.hdr.ack_num=htonl(ntohl(((struct sham_header*)buf)->seq_num)+1);
    ack2.hdr.flags=htons(SHAM_ACK);

    sendto(sock,&ack2,sizeof(struct sham_header),0,(struct sockaddr*)&srv,slen);
    log_event("SND ACK FOR FIN");

    fclose(in);
    close(sock);
    return 0;
}
