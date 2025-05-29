#define _GNU_SOURCE
#include "sensor.h"
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>

#define CHIP_NAME "gpiochip0"
#define SENSOR_COUNT 4

const int TRIG_PINS[SENSOR_COUNT] = {17, 27, 23, 5};
const int ECHO_PINS[SENSOR_COUNT] = {4, 22, 24, 6};
const int SERVO_PINS[SENSOR_COUNT] = {12, 13, 19, 26};

long get_microseconds()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
}

float measure_distance(struct gpiod_chip *chip, int trig_pin, int echo_pin)
{
    struct gpiod_line *trig = gpiod_chip_get_line(chip, trig_pin);
    struct gpiod_line *echo = gpiod_chip_get_line(chip, echo_pin);

    gpiod_line_request_output(trig, "trig", 0);
    gpiod_line_request_input(echo, "echo");

    gpiod_line_set_value(trig, 0);
    usleep(2);
    gpiod_line_set_value(trig, 1);
    usleep(10);
    gpiod_line_set_value(trig, 0);

    long timeout = get_microseconds() + 30000;
    while (gpiod_line_get_value(echo) == 0 && get_microseconds() < timeout)
        ;
    long pulse_start = get_microseconds();
    while (gpiod_line_get_value(echo) == 1 && get_microseconds() < timeout)
        ;
    long pulse_end = get_microseconds();

    gpiod_line_release(trig);
    gpiod_line_release(echo);

    long duration = pulse_end - pulse_start;
    return duration / 58.0f;
}

int run_sensor_sequence()
{
    struct gpiod_chip *chip = gpiod_chip_open_by_name(CHIP_NAME);
    if (!chip)
    {
        perror("gpiod_chip_open_by_name 실패");
        return 0;
    }

    float previous_distances[SENSOR_COUNT] = {0};
    int triggered = 0;

    for (int i = 0; i < SENSOR_COUNT; i++)
    {
        float dist = measure_distance(chip, TRIG_PINS[i], ECHO_PINS[i]);
        printf("센서 %d 거리: %.2f cm → ", i + 1, dist);

        float diff = fabs(dist - previous_distances[i]);
        previous_distances[i] = dist;

        if (dist <= 15.0 && diff >= 5.0)
        {
            triggered = 1; // 조건 충족
        }
        else
            usleep(200000);
    }

    printf("---------------\n");
    gpiod_chip_close(chip);
    return triggered;
}