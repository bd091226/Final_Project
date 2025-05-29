#ifndef SENSOR_H
#define SENSOR_H

#include <gpiod.h>

extern struct gpiod_chip *chip;

// 특정 거리 측정 및 조건 판단 (조건 만족하면 1 반환, 아니면 0)
int move_distance(struct gpiod_chip *chip, int sensor_index, float *last_distance);

// 특정 서보모터를 90도 회전 후 0도로 복귀
void move_servo(struct gpiod_chip *chip, int servo_index);

#endif