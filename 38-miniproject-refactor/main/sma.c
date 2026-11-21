#include "sma.h"

void sma_init(sma_t *f, int window)
{
    if (window < 1) window = 1;
    if (window > SMA_MAX_WINDOW) window = SMA_MAX_WINDOW;

    f->window = window;
    f->idx = 0;
    f->count = 0;
    f->sum = 0;
}

int sma_push(sma_t *f, int value)
{
    if (f->count < f->window) {
        f->sum += value;
        f->count++;
    } else {
        /* віднімаємо те, що випадає з вікна — сума перераховується за O(1) */
        f->sum += value - f->buf[f->idx];
    }

    f->buf[f->idx] = value;
    f->idx = (f->idx + 1) % f->window;

    return (int)(f->sum / f->count);
}
