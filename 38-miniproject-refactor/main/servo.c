#include "servo.h"

#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>

#define SERVO_PIN           GPIO_NUM_14
#define SERVO_FREQ_HZ       50
#define SERVO_PERIOD_US     (1000000 / SERVO_FREQ_HZ)   /* 20000 мкс */
#define SERVO_RESOLUTION    LEDC_TIMER_12_BIT
#define SERVO_MAX_DUTY      ((1 << SERVO_RESOLUTION) - 1)
#define SERVO_TIMER         LEDC_TIMER_0
#define SERVO_CHANNEL       LEDC_CHANNEL_0

#define SERVO_MIN_US        500
#define SERVO_MAX_US        2500

static const char *TAG = "servo";

void servo_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = SERVO_TIMER,
        .duty_resolution = SERVO_RESOLUTION,
        .freq_hz         = SERVO_FREQ_HZ,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t channel_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = SERVO_CHANNEL,
        .timer_sel  = SERVO_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = SERVO_PIN,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));
}

void servo_write_deg(int deg)
{
    if (deg < SERVO_MIN_DEG) deg = SERVO_MIN_DEG;
    if (deg > SERVO_MAX_DEG) deg = SERVO_MAX_DEG;

    int pulse_us = SERVO_MIN_US
                 + (SERVO_MAX_US - SERVO_MIN_US) * (deg - SERVO_MIN_DEG)
                   / (SERVO_MAX_DEG - SERVO_MIN_DEG);

    /* множення перед діленням, інакше цілочисельна арифметика з'їдає точність */
    uint32_t duty = (uint32_t)pulse_us * SERVO_MAX_DUTY / SERVO_PERIOD_US;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL));

    ESP_LOGI(TAG, "deg=%3d  pulse=%4d us  duty=%4lu", deg, pulse_us, duty);
}
