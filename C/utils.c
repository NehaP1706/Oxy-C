#include "shop.h"

// ---------- Utilities ----------
long program_start_sec() {
    static struct timeval start;
    static int inited = 0;
    if (!inited) { gettimeofday(&start, NULL); inited = 1; }
    struct timeval now;
    gettimeofday(&now, NULL);
    return now.tv_sec - start.tv_sec;
}
void print_time_pref() {
    printf("%ld ", program_start_sec());
}

// ---------- Logging helpers ----------
void log_customer_action(int id, const char *action) {
    print_time_pref();
    printf("Customer %d %s\n", id, action);
    fflush(stdout);
}
void log_chef_action(int id, const char *action) {
    print_time_pref();
    printf("Chef %d %s\n", id, action);
    fflush(stdout);
}