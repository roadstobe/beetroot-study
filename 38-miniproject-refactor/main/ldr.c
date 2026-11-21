#include "ldr.h"

#include <esp_adc/adc_oneshot.h>
#include <esp_adc/adc_cali.h>
#include <esp_adc/adc_cali_scheme.h>
#include <esp_log.h>

#define ADC_UNIT_USED   ADC_UNIT_1
#define ADC_CHAN        ADC_CHANNEL_8       /* GPIO9 */
#define ADC_ATTEN_USED  ADC_ATTEN_DB_12
#define ADC_BITS        ADC_BITWIDTH_12

static const char *TAG = "ldr";

static adc_oneshot_unit_handle_t s_adc;
static adc_cali_handle_t s_cali;

void ldr_init(void)
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

int ldr_read_mv(void)
{
    int raw = 0;
    int mv = 0;

    ESP_ERROR_CHECK(adc_oneshot_read(s_adc, ADC_CHAN, &raw));
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(s_cali, raw, &mv));

    ESP_LOGD(TAG, "raw=%d mv=%d", raw, mv);

    return mv;
}
