#include "shop.h"

void q_init(Queue *q) { q->head = q->tail = NULL; q->size = 0; }
void q_push(Queue *q, Customer *c) {
    c->next = NULL;
    if (!q->tail) q->head = q->tail = c;
    else { q->tail->next = c; q->tail = c; }
    q->size++;
}
Customer* q_pop(Queue *q) {
    if (!q->head) return NULL;
    Customer *c = q->head;
    q->head = c->next;
    if (!q->head) q->tail = NULL;
    c->next = NULL;
    q->size--;
    return c;
}
