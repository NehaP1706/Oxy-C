#include "utils.h"
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <errno.h>
#include <openssl/md5.h>

static FILE *log_file = NULL;

void _open_log_if_needed(const char *role) {
    char *env = getenv("RUDP_LOG");

    if (!env) return;

    if (log_file) return;

    if (strcmp(role, "server") == 0) 
    {
        log_file = fopen("server_log.txt","a");
    }
    else 
    {
        log_file = fopen("client_log.txt","a");
    }
}

uint64_t now_us() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec * 1000000ULL + (uint64_t)tv.tv_usec;
}

void log_event(const char *fmt, ...) {
    char *role = getenv("RUDP_ROLE");
    if (!role) role = "client"; // default
    _open_log_if_needed(role);
    
    if (!log_file) return;

    struct timeval tv;
    gettimeofday(&tv, NULL);
    char timestr[64];
    time_t cur = tv.tv_sec;
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&cur));

    fprintf(log_file, "[%s.%06ld] [LOG] ", timestr, (long)tv.tv_usec);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(log_file, fmt, ap);
    va_end(ap);
    fprintf(log_file, "\n");
    fflush(log_file);
}

int should_drop(double loss_rate) {
    if (loss_rate <= 0.0) return 0;

    double r = (double)rand() / (double)RAND_MAX;
    return r < loss_rate;
}