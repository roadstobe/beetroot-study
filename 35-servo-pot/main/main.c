/*
 * Модуль 3.5 / урок 35 — Керування сервоприводом за допомогою PWM
 *
 * Потенціометр на ADC1_CH8 (GPIO9) керує валом серво на GPIO14 у пропорції 1:1:
 * на скільки градусів повернули ручку — на стільки ж повертається вал.
 *
 * Потенціометр має більший хід (270°), ніж серво (180°), тому працює тільки
 * спільна частина — перші 180° ручки. Далі вал стоїть у крайньому положенні.
 */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/ledc.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>

#define POT_UNIT            ADC_UNIT_1
#define POT_CHANNEL         ADC_CHANNEL_8       /* GPIO9 */
#define POT_ATTEN           ADC_ATTEN_DB_12
#define POT_BITWIDTH        ADC_BITWIDTH_12
#define POT_MAX_CODE        4095

/* повний механічний хід потенціометра та серво */
#define POT_RANGE_DEG       270
#define SERVO_RANGE_DEG     180

/* 1:1 можливе лише там, де діапазони збігаються — решта ходу ручки не бере участі */
#define USABLE_RANGE_DEG    (POT_RANGE_DEG < SERVO_RANGE_DEG ? POT_RANGE_DEG : SERVO_RANGE_DEG)

#define SERVO_PIN           GPIO_NUM_14
#define SERVO_FREQ_HZ       50
#define SERVO_PERIOD_US     (1000000 / SERVO_FREQ_HZ)   /* 20000 мкс */
#define SERVO_RESOLUTION    LEDC_TIMER_12_BIT
#define SERVO_MAX_DUTY      ((1 << SERVO_RESOLUTION) - 1)
#define SERVO_TIMER         LEDC_TIMER_0
#define SERVO_CHANNEL       LEDC_CHANNEL_0
#define SERVO_MIN_US        500     /* крайнє ліве положення */
#define SERVO_MAX_US        2500    /* крайнє праве */

/* ADC шумить на молодших бітах, без згладжування вал дрібно смикається */
#define SMA_WINDOW          8
#define PERIOD_MS           50

static const char *TAG = "servo_pot";

static adc_oneshot_unit_handle_t s_adc;

static int s_buf[SMA_WINDOW];
static int s_idx;
static int s_count;
static long s_sum;

static int sma_push(int value)
{
    if (s_count < SMA_WINDOW) {
        s_sum += value;
        s_count++;
    } else {
        s_sum += value - s_buf[s_idx];
    }
    s_buf[s_idx] = value;
    s_idx = (s_idx + 1) % SMA_WINDOW;

    return (int)(s_sum / s_count);
}

static int clamp(int value, int min, int max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static void pot_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = POT_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = POT_ATTEN,
        .bitwidth = POT_BITWIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, POT_CHANNEL, &chan_cfg));
}

static void servo_init(void)
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

/* кут рахується від крайнього лівого положення вала */
static void servo_write_deg(int deg)
{
    deg = clamp(deg, 0, SERVO_RANGE_DEG);

    int pulse_us = SERVO_MIN_US
                 + (SERVO_MAX_US - SERVO_MIN_US) * deg / SERVO_RANGE_DEG;

    uint32_t duty = (uint32_t)pulse_us * SERVO_MAX_DUTY / SERVO_PERIOD_US;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL));
}

void app_main(void)
{
    pot_init();
    servo_init();

    ESP_LOGI(TAG, "потенціометр %d°, серво %d°, робочий діапазон %d°",
             POT_RANGE_DEG, SERVO_RANGE_DEG, USABLE_RANGE_DEG);

    while (1) {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc, POT_CHANNEL, &raw));

        int avg = sma_push(raw);

        /* положення ручки в градусах її власного ходу */
        int pot_deg = avg * POT_RANGE_DEG / POT_MAX_CODE;

        /* 1:1 — той самий градус, але не далі спільного діапазону */
        int servo_deg = clamp(pot_deg, 0, USABLE_RANGE_DEG);

        servo_write_deg(servo_deg);

        ESP_LOGI(TAG, "ручка=%3d°  вал=%3d°%s",
                 pot_deg, servo_deg,
                 (pot_deg > USABLE_RANGE_DEG) ? "  (поза діапазоном)" : "");

        vTaskDelay(pdMS_TO_TICKS(PERIOD_MS));
    }
}
