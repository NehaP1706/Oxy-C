
//////////////////////////////// LLM Generated Code Begins //////////////////////////////////////

#include "shop.h"

// ---------- Chef thread ----------
void *chef_thread(void *arg) {
    int chef_id = (int)(intptr_t)arg;

    while (!shutdown_flag) {
        Customer *toPay = NULL;
        Customer *toBake = NULL;

        // pick work: payments have priority
        pthread_mutex_lock(&lock);
        while (payment_queue.size == 0 && sofa_queue.size == 0) {
            // nothing to do, wait
            if (shutdown_flag) {
                pthread_mutex_unlock(&lock);
                break;
            }
            pthread_cond_wait(&chef_cv, &lock);
        }

        // payment priority
        if (payment_queue.size > 0) {
            toPay = q_pop(&payment_queue);
            // mark that this chef will accept payment for 'toPay'
            pthread_mutex_unlock(&lock);

            // Lock payment_mutex to serialize accepting payment
            pthread_mutex_lock(&payment_mutex);

            // perform accept payment (2 seconds) - atomic action
            log_chef_action(chef_id, "accepts payment", toPay);
            sleep(2);

            // signal customer payment accepted
            sem_post(&toPay->sem_payment_ok);

            // Unlock payment_mutex so next chef can accept payment
            pthread_mutex_unlock(&payment_mutex);

            // after finishing payment, continue loop
            continue;
        }

        // if no payments, check sofa queue to bake
        if (sofa_queue.size > 0) {
            toBake = q_pop(&sofa_queue);
            // Note: sofa seat remains reserved for this customer until they leave.
            pthread_mutex_unlock(&lock);

            // signal customer to start getcake concurrently
            sem_post(&toBake->sem_served);

            // wait for customer's signal to start baking
            sem_wait(&toBake->sem_start_bake);

            log_chef_action(chef_id, "bakes", toBake);
            sleep(2);

            // baking finished, inform customer cake ready
            sem_post(&toBake->sem_bake_done);

            continue;
        }
        // unlock if fell through
        pthread_mutex_unlock(&lock);
    }
    return NULL;
}

// ---------- Customer thread ----------
void *customer_thread(void *arg) {
    Customer *c = (Customer *)arg;

    // enterofficebakery (we simulate action time)
    log_customer_action(c->id, "enters");
    sleep(1); // customer action = 1s

    pthread_mutex_lock(&lock);
    // capacity check done before creating thread — but double-check here:
    if (current_customers > MAX_CAPACITY) {
        // bakery full, leaves immediately
        pthread_mutex_unlock(&lock);
        log_customer_action(c->id, "leaves");
        // cleanup
        sem_destroy(&c->sem_served);
        sem_destroy(&c->sem_bake_done);
        sem_destroy(&c->sem_payment_ok);
        free(c);
        return NULL;
    }
    // customer is now inside
    // current_customers++;

    // try to sit on sofa immediately if seat free
    if (sofa_occupied < SOFA_SEATS) {
        // take seat
        sofa_occupied++;
        pthread_mutex_unlock(&lock);

        // sitOnSofa action (1 sec)
        log_customer_action(c->id, "sits");
        sleep(1);

        // join sofa queue (wait to be served)
        pthread_mutex_lock(&lock);
        q_push(&sofa_queue, c);
        pthread_cond_broadcast(&chef_cv); // signal chefs that work available
        pthread_mutex_unlock(&lock);
    } else {
        // sofa full -> stand
        // join standing_queue and wait for a seat
        q_push(&standing_queue, c);
        log_customer_action(c->id, "stands");
        // wait until signaled that a seat is free and it's our turn (FIFO)
        while (1) {
            pthread_cond_wait(&standing_cv, &lock);
            // when signaled, check if we are at head of standing queue and if a sofa seat is available
            if (standing_queue.head == c && sofa_occupied < SOFA_SEATS) {
                // remove from standing_queue head (we are head)
                Customer *popped = q_pop(&standing_queue);
                if (popped != c) {
                    // shouldn't happen
                    fprintf(stderr, "Error: standing queue mismatch\n");
                }
                // occupy seat
                sofa_occupied++;
                // release lock and do sit action
                pthread_mutex_unlock(&lock);

                // sitOnSofa action
                log_customer_action(c->id, "sits");
                sleep(1);

                // join sofa queue
                pthread_mutex_lock(&lock);
                q_push(&sofa_queue, c);
                pthread_cond_broadcast(&chef_cv);
                pthread_mutex_unlock(&lock);
                break;
            }
            // otherwise continue waiting
        }
    }

    // now wait to be served: chef will sem_post(sem_served) to indicate they started serving (so that getcake runs concurrently)
    sem_wait(&c->sem_served);
    // getcake (1 sec) - must happen concurrently with chef's bake
    log_customer_action(c->id, "requests cake");
    sleep(1);

    // signal chef to start baking
    sem_post(&c->sem_start_bake);

    // wait for chef to complete baking (2 sec)
    sem_wait(&c->sem_bake_done);

    // after cake finished -> pay (1s)
    log_customer_action(c->id, "pays");
    sleep(1);

    // after pay action is done, we add ourselves to payment_queue and wait for an available chef to accept payment
    pthread_mutex_lock(&lock);
    q_push(&payment_queue, c);
    pthread_cond_broadcast(&chef_cv); // notify chefs (they prioritize payment)
    pthread_mutex_unlock(&lock);

    // wait until some chef accepts and posts sem_payment_ok
    sem_wait(&c->sem_payment_ok);

    // now payment has been accepted by a chef (chef spent 2s)
    // customer leaves now; free sofa seat and decrement capacity
    pthread_mutex_lock(&lock);
    sofa_occupied--;
    current_customers--;
    // free a sofa seat, so if anybody is standing, wake them (the earliest standing customer)
    pthread_cond_broadcast(&standing_cv);
    // also notify chefs in case they were waiting for tasks
    pthread_cond_broadcast(&chef_cv);
    pthread_mutex_unlock(&lock);

    log_customer_action(c->id, "leaves");

    // cleanup
    sem_destroy(&c->sem_served);
    sem_destroy(&c->sem_bake_done);
    sem_destroy(&c->sem_payment_ok);
    free(c);
    return NULL;
}

//////////////////////////////// LLM Generated Code Ends //////////////////////////////////////

