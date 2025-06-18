#ifndef SENSOR_H
#define SENSOR_H

#include <gpiod.h>

extern struct gpiod_chip *chip;

// 초음파 센서 핀 초기화
void init_ultrasonic_pins(struct gpiod_chip *chip);

// 거리 측정
float measure_distance(struct gpiod_chip *chip);

// 장애물 감지
int check_obstacle(struct gpiod_chip *chip);

#endif
