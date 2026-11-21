#pragma once

#define SMA_MAX_WINDOW 32

/* стан фільтра тримає сам викликач — можна мати кілька незалежних фільтрів */
typedef struct {
    int  buf[SMA_MAX_WINDOW];
    int  window;
    int  idx;
    int  count;
    long sum;
} sma_t;

void sma_init(sma_t *f, int window);
int  sma_push(sma_t *f, int value);
