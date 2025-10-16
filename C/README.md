## Instructions on how to run:
```c
make all
./main
```

## Assumptions:
1. A customer must sit in order to request for cake.
2. Any customer can pay at any time, but the payment is processed and accepted only when the register gets free at the appropriate time.
3. If the register is empty, any payment made is processed after a second, else more than one second has transpired, so accepting happens as and when the register is ready.
4. The action of leaving takes 1 second and therefore, if a customer leaves at x, the next customer can sit at x+1.
5. I shall print when the customer decides to stand, as it helps in debugging and is more intuitive.
6. If the Bakery capacity is reached, no customer enters and therefore no "enters" or "leaves" log messages are printed.