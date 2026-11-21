/*
 * Модуль 3.4 / урок 34 — Програвання звуку на зумері (PWM)
 *
 * Завдання 2: синтезатор на кілька нот, що грає просту мелодію.
 * Пасивний зумер на GPIO16, через LEDC. Висота ноти = частота таймера,
 * гучність не чіпаємо — duty тримаємо 50%.
 */
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/ledc.h>
#include <esp_log.h>

#define BUZZER_PIN      GPIO_NUM_16
#define BUZZER_TIMER    LEDC_TIMER_0
#define BUZZER_CHANNEL  LEDC_CHANNEL_0
#define BUZZER_RES      LEDC_TIMER_10_BIT
#define BUZZER_MAX_DUTY ((1 << BUZZER_RES) - 1)

/* меандр: половину періоду високий рівень, половину низький */
#define BUZZER_DUTY     (BUZZER_MAX_DUTY / 2)

#define NOTE_MS         400     /* тривалість ноти */
#define GAP_MS          60      /* пауза, щоб дві однакові ноти не злились */
#define MELODY_PAUSE_MS 1500

/* частоти четвертої октави, Гц */
#define NOTE_C  262
#define NOTE_D  294
#define NOTE_E  330
#define NOTE_F  349
#define NOTE_G  392
#define NOTE_A  440
#define NOTE_REST 0

static const char *TAG = "buzzer";

/* Twinkle Twinkle Little Star: C C G G A A G */
static const int s_melody[] = {
    NOTE_C, NOTE_C, NOTE_G, NOTE_G, NOTE_A, NOTE_A, NOTE_G,
};

#define MELODY_LEN (sizeof(s_melody) / sizeof(s_melody[0]))

static void buzzer_init(void)
{
    ledc_timer_config_t timer_cfg = {
        .speed_mode      = LEDC_LOW_SPEED_MODE,
        .timer_num       = BUZZER_TIMER,
        .duty_resolution = BUZZER_RES,
        .freq_hz         = NOTE_C,          /* будь-яка стартова, далі міняємо */
        .clk_cfg         = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_cfg));

    ledc_channel_config_t channel_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel    = BUZZER_CHANNEL,
        .timer_sel  = BUZZER_TIMER,
        .intr_type  = LEDC_INTR_DISABLE,
        .gpio_num   = BUZZER_PIN,
        .duty       = 0,
        .hpoint     = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&channel_cfg));
}

static void buzzer_set_duty(uint32_t duty)
{
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_LOW_SPEED_MODE, BUZZER_CHANNEL));
}

static void play_note(int freq_hz, int ms)
{
    if (freq_hz == NOTE_REST) {
        buzzer_set_duty(0);
    } else {
        ESP_ERROR_CHECK(ledc_set_freq(LEDC_LOW_SPEED_MODE, BUZZER_TIMER, freq_hz));
        buzzer_set_duty(BUZZER_DUTY);
    }
    vTaskDelay(pdMS_TO_TICKS(ms));

    /* тиша між нотами */
    buzzer_set_duty(0);
    vTaskDelay(pdMS_TO_TICKS(GAP_MS));
}

void app_main(void)
{
    buzzer_init();

    while (1) {
        ESP_LOGI(TAG, "граємо мелодію (%d нот)", (int)MELODY_LEN);

        for (int i = 0; i < MELODY_LEN; i++) {
            play_note(s_melody[i], NOTE_MS);
        }

        vTaskDelay(pdMS_TO_TICKS(MELODY_PAUSE_MS));
    }
}
