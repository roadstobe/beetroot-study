/*
 * Урок 38 — розбір і оптимізація мініпроєкту.
 *
 * Той самий сценарій: LDR керує кутом сервопривода.
 * Код розкладений по модулях: ldr / servo / sma, у main лишилась тільки логіка.
 */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "ldr.h"
#include "servo.h"
#include "sma.h"

#define LIGHT_MIN_MV        50
#define LIGHT_MAX_MV        3000

#define SMA_WINDOW          8
#define SUPERLOOP_DELAY_MS  100

static int map_range(int value, int in_min, int in_max, int out_min, int out_max)
{
    if (in_min == in_max) return out_max;

    if (value < in_min) value = in_min;
    if (value > in_max) value = in_max;

    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

void app_main(void)
{
    sma_t light_filter;

    ldr_init();
    servo_init();
    sma_init(&light_filter, SMA_WINDOW);

    while (1) {
        int mv = sma_push(&light_filter, ldr_read_mv());

        int deg = map_range(mv, LIGHT_MIN_MV, LIGHT_MAX_MV,
                            SERVO_MIN_DEG, SERVO_MAX_DEG);

        servo_write_deg(deg);

        vTaskDelay(pdMS_TO_TICKS(SUPERLOOP_DELAY_MS));
    }
}
