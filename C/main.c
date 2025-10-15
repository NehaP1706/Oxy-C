//////////////////////////////// LLM Generated Code Begins //////////////////////////////////////
#include "shop.h"

// ---------- Global bakery state ----------
pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t chef_cv = PTHREAD_COND_INITIALIZER;     // chefs wait on this if nothing to do
pthread_cond_t standing_cv = PTHREAD_COND_INITIALIZER; // standing customers wait for seat
pthread_mutex_t payment_mutex = PTHREAD_MUTEX_INITIALIZER;
int current_customers = 0; // overall count inside bakery (max MAX_CAPACITY)
int sofa_occupied = 0;     // current occupied sofa seats (reserved until customer leaves)

Queue sofa_queue;      // customers currently on sofa (FIFO) -> waiting to be served by chefs
Queue standing_queue;  // customers standing waiting for an available sofa
Queue payment_queue;   // customers who have finished pay action and waiting for chef to accept payment

int shutdown_flag = 0; // for chef shutdown

int main() {
    // initialize queues
    q_init(&sofa_queue);
    q_init(&standing_queue);
    q_init(&payment_queue);

    // start chef threads
    pthread_t chefs[NUM_CHEFS];
    for (int i = 0; i < NUM_CHEFS; ++i) {
        pthread_create(&chefs[i], NULL, chef_thread, (void*)(intptr_t)(i+1));
    }

    // Read arrival lines until <EOF>
    char line[256];
    Arrival *arr_head = NULL, *arr_tail = NULL;
    int total_customers = 0;
    while (fgets(line, sizeof(line), stdin)) {
        if (strncmp(line, "<EOF>", 5) == 0) break;
        if (strlen(line) == 0) continue;
        int ts, id;
        char word[64];
        if (sscanf(line, "%d %63s %d", &ts, word, &id) == 3) {
            if (strcmp(word, "Customer") != 0) {
                fprintf(stderr, "Skipping invalid line: %s", line);
                continue;
            }

            Arrival *a = malloc(sizeof(Arrival));
            a->ts = ts;
            a->id = id;
            a->next = NULL;
            if (!arr_tail) arr_head = arr_tail = a;
            else { arr_tail->next = a; arr_tail = a; }
            total_customers++;
        } else {
            fprintf(stderr, "Skipping invalid line: %s", line);
        }
    }

    // array to store customer thread handles
    pthread_t *cust_threads = malloc(sizeof(pthread_t) * total_customers);
    int cust_count = 0;

    // schedule customers according to timestamps
    gettimeofday(&(struct timeval){0}, NULL); // warm-up call

    Arrival *cur = arr_head;
    long start_time = program_start_sec(); (void)start_time;

    while (cur) {
        int target = cur->ts;
        long now = program_start_sec();
        if (target > now) sleep(target - now);

        pthread_mutex_lock(&lock);
        if (current_customers >= MAX_CAPACITY) {
            pthread_mutex_unlock(&lock);
            print_time_pref();
            printf("Customer %d leaves\n", cur->id);
            fflush(stdout);
            Arrival *tmp = cur;
            cur = cur->next;
            free(tmp);
            continue;
        }

        // create Customer struct and thread
        Customer *c = malloc(sizeof(Customer));
        c->id = cur->id;
        sem_init(&c->sem_served, 0, 0);
        sem_init(&c->sem_bake_done, 0, 0);
        sem_init(&c->sem_payment_ok, 0, 0);
        sem_init(&c->sem_start_bake, 0, 0);
        c->next = NULL;
        current_customers++;

        if (pthread_create(&cust_threads[cust_count], NULL, customer_thread, c) != 0) {
            perror("pthread_create customer");
            exit(1);
        }
        cust_count++;
        pthread_mutex_unlock(&lock);

        Arrival *tmp = cur;
        cur = cur->next;
        free(tmp);
    }

    // === Wait for all customer threads ===
    for (int i = 0; i < cust_count; i++) {
        pthread_join(cust_threads[i], NULL);
    }
    free(cust_threads);

    // === Once all customers have left, shut down chefs ===
    pthread_mutex_lock(&lock);
    shutdown_flag = 1;
    pthread_cond_broadcast(&chef_cv);
    pthread_mutex_unlock(&lock);

    for (int i = 0; i < NUM_CHEFS; ++i) {
        pthread_join(chefs[i], NULL);
    }

    return 0;
}
//////////////////////////////// LLM Generated Code Ends //////////////////////////////////////
