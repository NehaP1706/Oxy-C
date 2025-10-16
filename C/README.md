## Instructions on how to run:
```c
make all
./main
```

## Assumptions:
1. A customer must sit in order to request for cake.
2. Any customer can pay at any time, but the payment is processed and accepted only when the register gets free at the appropriate time.
3. If the register is empty, any payment made is processed after a second, else more than one second has transpired, so accepting happens as and when the register is ready.