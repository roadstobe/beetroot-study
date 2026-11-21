#pragma once

#define SERVO_MIN_DEG 10
#define SERVO_MAX_DEG 170

void servo_init(void);
void servo_write_deg(int deg);
