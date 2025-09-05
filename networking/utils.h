#include <stdio.h>
#include <stdint.h>
#include <sys/time.h>

void log_event(const char *fmt, ...);
int should_drop(double loss_rate);
uint64_t now_us();