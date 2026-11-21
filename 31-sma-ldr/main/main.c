/*
 * Модуль 3.1 / урок 31 — Проста ковзна середня (SMA)
 *
 * LDR у подільнику напруги на ADC1_CH8 (GPIO9), світлодіод на GPIO17.
 * Значення ADC згладжуються ковзною середньою, вмикання/вимикання —
 * через два пороги (гістерезис), щоб світлодіод не мерехтів.
 */
#include <stdbool.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_log.h>

#define LDR_UNIT        ADC_UNIT_1
#define LDR_CHANNEL     ADC_CHANNEL_8   /* GPIO9 */
#define LDR_ATTEN       ADC_ATTEN_DB_12
#define LDR_BITWIDTH    ADC_BITWIDTH_12

#define LED_PIN         GPIO_NUM_17

#define SMA_WINDOW      16
#define PERIOD_MS       50

/*
 * Пороги у сирих значеннях ADC. Схема: 3V3 — LDR — (ADC) — 10к — GND,
 * тож більше світла → менший опір LDR → більша напруга на вході.
 * Отже темрява = мале значення.
 * Проміжок між порогами і є гістерезис: поки середнє всередині — стан не міняється.
 */
#define DARK_LEVEL      1200
#define LIGHT_LEVEL     1800

static const char *TAG = "sma_ldr";

static adc_oneshot_unit_handle_t s_adc;

static int s_buf[SMA_WINDOW];
static int s_idx;
static int s_count;
static long s_sum;

/* ковзна середня: тримаємо суму вікна, а не рахуємо її щоразу наново */
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

static void ldr_init(void)
{
    adc_oneshot_unit_init_cfg_t unit_cfg = {
        .unit_id = LDR_UNIT,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&unit_cfg, &s_adc));

    adc_oneshot_chan_cfg_t chan_cfg = {
        .atten = LDR_ATTEN,
        .bitwidth = LDR_BITWIDTH,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(s_adc, LDR_CHANNEL, &chan_cfg));
}

static void led_init(void)
{
    gpio_reset_pin(LED_PIN);
    ESP_ERROR_CHECK(gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT));
    ESP_ERROR_CHECK(gpio_set_level(LED_PIN, 0));
}

void app_main(void)
{
    ldr_init();
    led_init();

    bool led_on = false;

    while (1) {
        int raw = 0;
        ESP_ERROR_CHECK(adc_oneshot_read(s_adc, LDR_CHANNEL, &raw));

        int avg = sma_push(raw);

        if (!led_on && avg < DARK_LEVEL) {
            led_on = true;
        } else if (led_on && avg > LIGHT_LEVEL) {
            led_on = false;
        }

        ESP_ERROR_CHECK(gpio_set_level(LED_PIN, led_on));

        ESP_LOGI(TAG, "raw=%4d  sma=%4d  led=%s", raw, avg, led_on ? "ON" : "OFF");

        vTaskDelay(pdMS_TO_TICKS(PERIOD_MS));
    }
}
