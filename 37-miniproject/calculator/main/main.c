/*
 * Урок 37 — мініпроєкт, завдання «Калькулятор».
 *
 * Енкодер EC11: A=GPIO10, B=GPIO11, кнопка=GPIO12.
 * Обертання задає значення, натискання підтверджує:
 *   перший операнд → операція → другий операнд → результат.
 *
 * Формат у консолі: 234 -> 234 + -> 234 + 77 -> 234 + 77 = 311
 */
#include <stdio.h>
#include <string.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <driver/gpio.h>
#include <driver/pulse_cnt.h>
#include <esp_log.h>

#define ENC_A_PIN           GPIO_NUM_10
#define ENC_B_PIN           GPIO_NUM_11
#define ENC_BTN_PIN         GPIO_NUM_12

#define PCNT_HIGH_LIMIT     2000
#define PCNT_LOW_LIMIT      (-2000)
#define GLITCH_FILTER_NS    1000

/* EC11 дає 4 зміни стану на один клік */
#define STEPS_PER_DETENT    4

#define OPERAND_MIN         0
#define OPERAND_MAX         999

#define POLL_MS             5
#define BTN_STABLE_MS       40

typedef enum {
    ST_OPERAND1,
    ST_OPERATION,
    ST_OPERAND2,
    ST_RESULT,
} state_t;

static const char OPS[] = { '+', '-', '*', '/' };
#define OP_COUNT ((int)(sizeof(OPS) / sizeof(OPS[0])))

static pcnt_unit_handle_t s_pcnt;

static int s_operand1;
static int s_operand2;
static int s_op_idx;
static state_t s_state = ST_OPERAND1;

static int s_printed;   /* скільки символів зараз у рядку */

/* ---------- енкодер ---------- */

static void encoder_init(void)
{
    pcnt_unit_config_t unit_cfg = {
        .high_limit = PCNT_HIGH_LIMIT,
        .low_limit = PCNT_LOW_LIMIT,
    };
    ESP_ERROR_CHECK(pcnt_new_unit(&unit_cfg, &s_pcnt));

    /* апаратний фільтр брязкоту — короткі імпульси не рахуються */
    pcnt_glitch_filter_config_t filter_cfg = {
        .max_glitch_ns = GLITCH_FILTER_NS,
    };
    ESP_ERROR_CHECK(pcnt_unit_set_glitch_filter(s_pcnt, &filter_cfg));

    /* два канали: кожен рахує свій пін, а напрям бере з протилежного */
    pcnt_chan_config_t chan_a_cfg = {
        .edge_gpio_num = ENC_A_PIN,
        .level_gpio_num = ENC_B_PIN,
    };
    pcnt_channel_handle_t chan_a = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt, &chan_a_cfg, &chan_a));

    pcnt_chan_config_t chan_b_cfg = {
        .edge_gpio_num = ENC_B_PIN,
        .level_gpio_num = ENC_A_PIN,
    };
    pcnt_channel_handle_t chan_b = NULL;
    ESP_ERROR_CHECK(pcnt_new_channel(s_pcnt, &chan_b_cfg, &chan_b));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_a,
        PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_a,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_channel_set_edge_action(chan_b,
        PCNT_CHANNEL_EDGE_ACTION_INCREASE, PCNT_CHANNEL_EDGE_ACTION_DECREASE));
    ESP_ERROR_CHECK(pcnt_channel_set_level_action(chan_b,
        PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE));

    ESP_ERROR_CHECK(pcnt_unit_enable(s_pcnt));
    ESP_ERROR_CHECK(pcnt_unit_clear_count(s_pcnt));
    ESP_ERROR_CHECK(pcnt_unit_start(s_pcnt));
}

static void button_init(void)
{
    gpio_config_t cfg = {
        .pin_bit_mask = 1ULL << ENC_BTN_PIN,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&cfg));
}

/* ---------- вивід ---------- */

static int clamp(int value, int min, int max)
{
    if (value < min) return min;
    if (value > max) return max;
    return value;
}

static void build_line(char *out, size_t size)
{
    switch (s_state) {
    case ST_OPERAND1:
        snprintf(out, size, "%d", s_operand1);
        break;
    case ST_OPERATION:
        snprintf(out, size, "%d %c ", s_operand1, OPS[s_op_idx]);
        break;
    case ST_OPERAND2:
        snprintf(out, size, "%d %c %d", s_operand1, OPS[s_op_idx], s_operand2);
        break;
    case ST_RESULT: {
        char result[16];

        if (OPS[s_op_idx] == '/' && s_operand2 == 0) {
            snprintf(result, sizeof(result), "ділення на 0");
        } else {
            int value = 0;
            switch (OPS[s_op_idx]) {
            case '+': value = s_operand1 + s_operand2; break;
            case '-': value = s_operand1 - s_operand2; break;
            case '*': value = s_operand1 * s_operand2; break;
            case '/': value = s_operand1 / s_operand2; break;
            }
            snprintf(result, sizeof(result), "%d", value);
        }

        snprintf(out, size, "%d %c %d = %s",
                 s_operand1, OPS[s_op_idx], s_operand2, result);
        break;
    }
    }
}

/* стираємо попередній рядок через \b і друкуємо новий на тому ж місці */
static void redraw(void)
{
    char line[64];
    build_line(line, sizeof(line));

    for (int i = 0; i < s_printed; i++) putchar('\b');
    for (int i = 0; i < s_printed; i++) putchar(' ');
    for (int i = 0; i < s_printed; i++) putchar('\b');

    fputs(line, stdout);
    fflush(stdout);

    s_printed = (int)strlen(line);
}

/* ---------- логіка ---------- */

static void apply_rotation(int delta)
{
    switch (s_state) {
    case ST_OPERAND1:
        s_operand1 = clamp(s_operand1 + delta, OPERAND_MIN, OPERAND_MAX);
        break;
    case ST_OPERATION:
        s_op_idx = ((s_op_idx + delta) % OP_COUNT + OP_COUNT) % OP_COUNT;
        break;
    case ST_OPERAND2:
        s_operand2 = clamp(s_operand2 + delta, OPERAND_MIN, OPERAND_MAX);
        break;
    case ST_RESULT:
        break;  /* результат обертанням не міняється */
    }
}

static void apply_press(void)
{
    switch (s_state) {
    case ST_OPERAND1:
        s_state = ST_OPERATION;
        break;
    case ST_OPERATION:
        s_state = ST_OPERAND2;
        break;
    case ST_OPERAND2:
        s_state = ST_RESULT;
        break;
    case ST_RESULT:
        /* новий приклад з чистого рядка */
        putchar('\n');
        fflush(stdout);
        s_printed = 0;
        s_operand1 = 0;
        s_operand2 = 0;
        s_op_idx = 0;
        s_state = ST_OPERAND1;
        break;
    }
}

void app_main(void)
{
    encoder_init();
    button_init();

    int last_detents = 0;
    bool btn_pressed = false;
    int btn_stable_ms = 0;

    redraw();

    while (1) {
        /* обертання */
        int count = 0;
        ESP_ERROR_CHECK(pcnt_unit_get_count(s_pcnt, &count));

        int detents = count / STEPS_PER_DETENT;
        if (detents != last_detents) {
            apply_rotation(detents - last_detents);
            last_detents = detents;
            redraw();
        }

        /* кнопка: натиснута = 0, бо підтяжка до 3V3 */
        bool level_pressed = (gpio_get_level(ENC_BTN_PIN) == 0);

        if (level_pressed == btn_pressed) {
            btn_stable_ms = 0;
        } else {
            btn_stable_ms += POLL_MS;
            if (btn_stable_ms >= BTN_STABLE_MS) {
                btn_pressed = level_pressed;
                btn_stable_ms = 0;

                if (btn_pressed) {      /* реагуємо на натискання, не на відпускання */
                    apply_press();
                    redraw();
                }
            }
        }

        vTaskDelay(pdMS_TO_TICKS(POLL_MS));
    }
}
