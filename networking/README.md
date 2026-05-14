## Instructions to run:
```c
make all
./server <port> [--chat] [loss_rate]
./client <server_ip> <server_port> --chat [loss_rate]
```

## Additional details:
- Loss rate is used to simulate real world traffic conditions.
- Chat mode allows transmission of bidirectional messages between the server and client.
- File transfer mode allows a reasonable file size (capped) to be pushed.
