/*
 * Модуль 3.2 / урок 32 — Калібрування даних з ADC
 *
 * Потенціометр ~10 кОм подільником на ADC1_CH8 (GPIO9).
 * Кожні 100 мс: сире значення, напруга за формулою, напруга з калібрування,
 * похибка між ними — рядком у таблицю в консолі.
 */
#include <stdlib.h>
#include <math.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>

#define ADC_UNIT_USED   ADC_UNIT_1
#define ADC_CHAN        ADC_CHANNEL_8   /* GPIO9 */
#define ADC_ATTEN_USED  ADC_ATTEN_DB_12
#define ADC_BITS        ADC_BITWIDTH_12

/* 12 біт → 4096 рівнів, максимальний код 4095 */
#define ADC_MAX_CODE    4095
/* повна шкала при атенюації 12 dB, з даташита ESP32-S3 */
#define ADC_VREF_MV     3100

#define PERIOD_MS       100

static const char *TAG = "adc_cali";

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;

static void adc_init(void)
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

    /* ESP32-S3 підтримує схему curve fitting */
    adc_cali_curve_fitting_config_t cali_cfg = {
        .unit_id = ADC_UNIT_USED,
        .chan = ADC_CHAN,
        .atten = ADC_ATTEN_USED,
        .bitwidth = ADC_BITS,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_cfg, &s_cali));
}

void app_main(void)
{
    adc_init();

    ESP_LOGI(TAG, "Vref=%d mV, атенюація=12 dB, розрядність=12 біт (макс. код %d)",
             ADC_VREF_MV, ADC_MAX_CODE);

    printf("\n RAW   U_manual(mV)   U_cali(mV)   Error(%%)\n");
    printf("------------------------------------------\n");

    while (1) {
        int raw = 0;
        int mv_cali = 0;

        ESP_ERROR_CHECK(adc_oneshot_read(s_adc, ADC_CHAN, &raw));
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali, raw, &mv_cali));

        /* лінійна формула: код × повна шкала / максимальний код */
        float mv_manual = (float)raw * ADC_VREF_MV / ADC_MAX_CODE;

        /* за еталон беремо кальібровану напругу */
        float error = (mv_cali != 0)
            ? fabsf(mv_manual - mv_cali) * 100.0f / mv_cali
            : 0.0f;

        printf("%4d      %7.1f      %7d      %5.2f\n", raw, mv_manual, mv_cali, error);

        vTaskDelay(pdMS_TO_TICKS(PERIOD_MS));
    }
}
