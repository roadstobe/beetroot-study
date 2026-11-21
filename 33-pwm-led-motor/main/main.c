/*
 * Модуль 3.3 / урок 33 — ШІМ (PWM)
 *
 * Потенціометр на ADC1_CH8 (GPIO9) керує одночасно:
 *   яскравістю світлодіода (GPIO17) і швидкістю двигуна (GPIO18).
 *
 * Світлодіод і двигун сидять на РІЗНИХ таймерах LEDC, тому їхні частоти
 * незалежні й не впливають одне на одного.
 */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>

#define POT_UNIT        ADC_UNIT_1
#define POT_CHANNEL     ADC_CHANNEL_8   /* GPIO9 */
#define POT_ATTEN       ADC_ATTEN_DB_12
#define POT_BITWIDTH    ADC_BITWIDTH_12
#define POT_MAX_CODE    4095

/* світлодіод: 5 кГц вистачає, щоб око не бачило мерехтіння */
#define LED_PIN         GPIO_NUM_17
#define LED_TIMER       LEDC_TIMER_0
#define LED_CHANNEL     LEDC_CHANNEL_0
#define LED_RES         LEDC_TIMER_12_BIT
#define LED_FREQ_HZ     5000
#define LED_MAX_DUTY    ((1 << LED_RES) - 1)

/* двигун: 20 кГц — вище чутності, щоб не пищав на низьких обертах */
#define MOTOR_PIN       GPIO_NUM_18
#define MOTOR_TIMER     LEDC_TIMER_1
#define MOTOR_CHANNEL   LEDC_CHANNEL_1
#define MOTOR_RES       LEDC_TIMER_10_BIT
#define MOTOR_FREQ_HZ   20000
#define MOTOR_MAX_DUTY  ((1 << MOTOR_RES) - 1)

#define PERIOD_MS       50

static const char *TAG = "pwm";

static adc_oneshot_unit_handle_t s_adc;

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

static void pwm_channel_init(ledc_timer_t timer, ledc_channel_t channel,
                             int gpio, ledc_timer_bit_t res, uint32_t freq_hz)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = timer,
        .duty_resolution = res,
        .freq_hz         = freq_hz,
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t channel_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = channel,
        .timer_sel  = timer,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = gpio,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));
}

static void pwm_set(ledc_channel_t channel, uint32_t duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, channel, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, channel));
}

void app_main(void)
{
    pot_init();
    pwm_channel_init(LED_TIMER, LED_CHANNEL, LED_PIN, LED_RES, LED_FREQ_HZ);
    pwm_channel_init(MOTOR_TIMER, MOTOR_CHANNEL, MOTOR_PIN, MOTOR_RES, MOTOR_FREQ_HZ);

    while (1) {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc, POT_CHANNEL, &raw));

        /* одне значення потенціометра — два різні діапазони duty */
        uint32_t led_duty = (uint32_t)raw * LED_MAX_DUTY / POT_MAX_CODE;
        uint32_t motor_duty = (uint32_t)raw * MOTOR_MAX_DUTY / POT_MAX_CODE;

        pwm_set(LED_CHANNEL, led_duty);
        pwm_set(MOTOR_CHANNEL, motor_duty);

        ESP_LOGI(TAG, "raw=%4d  led_duty=%4lu/%d  motor_duty=%4lu/%d",
                 raw, led_duty, LED_MAX_DUTY, motor_duty, MOTOR_MAX_DUTY);

        vTaskDelay(pdMS_TO_TICKS(PERIOD_MS));
    }
}
