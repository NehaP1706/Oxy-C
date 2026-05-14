# Components

The directory structure is as follows:
```c
.
├── bakery
│   ├── main.c
│   ├── Makefile
│   ├── queue.c
│   ├── README.md
│   ├── shop.h
│   ├── threads.c
│   └── utils.c
├── cshark
│   ├── capture.c
│   ├── cshark.c
│   ├── cshark.h
│   ├── inspect.c
│   ├── L3.c
│   ├── L4.c
│   ├── Makefile
│   ├── menu.c
│   ├── print.c
│   ├── README.md
│   └── utils.c
├── networking
│   ├── client.c
│   ├── Makefile
│   ├── server.c
│   ├── sham.h
│   ├── utils.c
│   └── utils.h
└── shell
    ├── include
    │   └── shell.h
    ├── Makefile
    └── src
        ├── atomic.c
        ├── bg.c
        ├── display.c
        ├── hop.c
        ├── log.c
        ├── parse.c
        ├── pipe.c
        ├── reveal.c
        ├── sequential.c
        ├── shell.c
        ├── signal.c
        ├── token.c
        └── util.c

7 directories, 39 files
```

This repository consists of a consolidated effort to implement several OS & Networking concepts with limited scopes.

## BAKERY

Bakery is a simulation that utilizes the concept of THREADING. Certain testcases bring out the inherent inconsistencies and non-deterministic nature of thread scheduling. Customers and Bakers are treated as threads with their own actions and relevant queues for sleep conditions. Customers can enter, sit on a sofa, order a cake, make a payment and leave the bakery while the bakers prepare cakes for the customers based on a FCFS basis and handle the singular register in the station. 

## CSHARK

Cshark implements a packet sniffer with live filters for different inspection requirements. Sufficient information is provided at each step with the help of a CLI menu.

## NETWORKING

Networking involves a standard client-server connection with extension to allow file transfer capabilities alongside message transmission.

## SHELL

Shell is a custom C-shell with a from-scratch implementations of several commands.
