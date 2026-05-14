//////////////////////////////// LLM Generated Code Begins //////////////////////////////////////

#ifndef SHOP_H
#define SHOP_H

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <string.h>
#include <sys/time.h>
#include <errno.h>

#define MAX_CAPACITY 25
#define SOFA_SEATS 4
#define NUM_CHEFS 4

// ---------- Global bakery state ----------
extern pthread_mutex_t lock;
extern pthread_cond_t chef_cv;     // chefs wait on this if nothing to do
extern pthread_cond_t standing_cv; // standing customers wait for seat
extern pthread_mutex_t payment_mutex;
extern int current_customers; // overall count inside bakery (max MAX_CAPACITY)
extern int sofa_occupied;     // current occupied sofa seats (reserved until customer leaves)

// ---------- Customer structure ----------
typedef struct Customer {
    int id;
    // semaphores/conds to sync with chef
    sem_t sem_served;        // posted by chef when starting to serve (so customer can 'getcake')
    sem_t sem_bake_done;     // posted by chef when baking is finished
    sem_t sem_payment_ok;    // posted by chef when payment accepted (allowed to leave)
    sem_t sem_start_bake;
    struct Customer *next;   // for queue linking
} Customer;

// ---------- FIFO queue helpers ----------
typedef struct Queue {
    Customer *head;
    Customer *tail;
    int size;
} Queue;

extern Queue sofa_queue;      // customers currently on sofa (FIFO) -> waiting to be served by chefs
extern Queue standing_queue;  // customers standing waiting for an available sofa
extern Queue payment_queue;   // customers who have finished pay action and waiting for chef to accept payment

// chefs threads will run forever (or until main exits)
extern int shutdown_flag; // not used for gentle shutdown in this sample

// ---------- Input scheduler ----------
typedef struct Arrival {
    int ts;
    int id;
    struct Arrival *next;
} Arrival;


void *chef_thread(void *arg);
void *customer_thread(void *arg);
void q_init(Queue *q);
void q_push(Queue *q, Customer *c);
Customer* q_pop(Queue *q);
long program_start_sec();
void print_time_pref();
void log_customer_action(int id, const char *action);
void log_chef_action(int id, const char *action, Customer *c);

#endif
//////////////////////////////// LLM Generated Code Ends //////////////////////////////////////
