/*
 * Урок 37 — мініпроєкт, порефакторений код.
 *
 * LDR на ADC1_CH8 (GPIO9) керує кутом сервопривода на GPIO14.
 * Яскравіше світло → більший кут.
 */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/ledc.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>

#define ADC_UNIT_USED       ADC_UNIT_1
#define ADC_CHAN            ADC_CHANNEL_8       /* GPIO9 */
#define ADC_ATTEN_USED      ADC_ATTEN_DB_12
#define ADC_BITS            ADC_BITWIDTH_12

#define SERVO_PIN           GPIO_NUM_14
#define SERVO_FREQ_HZ       50
#define SERVO_PERIOD_US     (1000000 / SERVO_FREQ_HZ)   /* 20000 мкс */
#define SERVO_RESOLUTION    LEDC_TIMER_12_BIT
#define SERVO_MAX_DUTY      ((1 << SERVO_RESOLUTION) - 1)
#define SERVO_TIMER         LEDC_TIMER_0
#define SERVO_CHANNEL       LEDC_CHANNEL_0

#define SERVO_MIN_DEG       10
#define SERVO_MAX_DEG       170
#define SERVO_MIN_US        500
#define SERVO_MAX_US        2500

#define LIGHT_MIN_MV        50
#define LIGHT_MAX_MV        3000

#define SMA_WINDOW          8
#define SUPERLOOP_DELAY_MS  100

static const char *TAG = "miniproject";

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;

/* ---------- ковзна середня ---------- */

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

/* ---------- допоміжне ---------- */

static int clamp(int value, int min, int max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static int map_range(int value, int in_min, int in_max, int out_min, int out_max)
{
    if (in_min == in_max) return out_max;

    value = clamp(value, in_min, in_max);

    return out_min + (value - in_min) * (out_max - out_min) / (in_max - in_min);
}

/* ---------- LDR ---------- */

static void ldr_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = ADC_UNIT_USED,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = ADC_ATTEN_USED,
        .bitwidth = ADC_BITS,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, ADC_CHAN, &chan_cfg));

    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_USED,
        .chan = ADC_CHAN,
        .atten = ADC_ATTEN_USED,
        .bitwidth = ADC_BITS,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali));
}

static int ldr_read_mv(void)
{
    int raw = 0;
    int mv = 0;

    ESP_ERROR_CHECK(adc_oneshot_read(s_adc, ADC_CHAN, &raw));
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali, raw, &mv));

    ESP_LOGD(TAG, "raw=%d mv=%d", raw, mv);

    return mv;
}

/* ---------- сервопривід ---------- */

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

static void servo_write_deg(int deg)
{
    deg = clamp(deg, SERVO_MIN_DEG, SERVO_MAX_DEG);

    int pulse_us = map_range(deg, SERVO_MIN_DEG, SERVO_MAX_DEG,
                             SERVO_MIN_US, SERVO_MAX_US);

    /* частка періоду, яку імпульс займає, у тих самих одиницях, що й duty */
    uint32_t duty = (uint32_t)pulse_us * SERVO_MAX_DUTY / SERVO_PERIOD_US;

    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, SERVO_CHANNEL));

    ESP_LOGI(TAG, "deg=%3d  pulse=%4d us  duty=%4lu", deg, pulse_us, duty);
}

/* ---------- superloop ---------- */

void app_main(void)
{
    ldr_init();
    servo_init();

    while (1) {
        int mv = sma_push(ldr_read_mv());
        int deg = map_range(mv, LIGHT_MIN_MV, LIGHT_MAX_MV,
                            SERVO_MIN_DEG, SERVO_MAX_DEG);

        servo_write_deg(deg);

        vTaskDelay(pdMS_TO_TICKS(SUPERLOOP_DELAY_MS));
    }
}
