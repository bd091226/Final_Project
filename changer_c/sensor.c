#define _GNU_SOURCE

#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define CHIP_NAME "gpiochip0"
#define SENSOR_COUNT 4

// physical 번호 1 ~ 40
const int TRIG_PINS[SENSOR_COUNT] = {17, 27, 23, 5};   // 11,13,16,29
const int ECHO_PINS[SENSOR_COUNT] = {4, 22, 24, 6};    // 7,15,18,31
const int SERVO_PINS[SENSOR_COUNT] = {16, 20, 21, 26}; // 36,38,40,37

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

void move_servo(struct gpiod_chip *chip, int servo_pin, int angle)
{
    struct gpiod_line *servo = gpiod_chip_get_line(chip, servo_pin);
    gpiod_line_request_output(servo, "servo", 0);

    int pulseWidth = 500 + angle * 11;
    int cycles = 20;

    for (int i = 0; i < cycles; i++)
    {
        gpiod_line_set_value(servo, 1);
        usleep(pulseWidth);
        gpiod_line_set_value(servo, 0);
        usleep(20000 - pulseWidth);
    }

    gpiod_line_release(servo);
    printf("[서보 %d] 각도: %d°\n", servo_pin, angle);
}

int main()
{
    struct gpiod_chip *chip = gpiod_chip_open_by_name(CHIP_NAME);
    if (!chip)
    {
        perror("gpiod_chip_open_by_name 실패");
        return 1;
    }

    while (1)
    {
        for (int i = 0; i < SENSOR_COUNT; i++)
        {
            float dist = measure_distance(chip, TRIG_PINS[i], ECHO_PINS[i]);
            printf("센서 %d 거리: %.2f cm → ", i + 1, dist);

            if (dist < 5.0)
            {
                printf("서보 %d ON\n", i + 1);
                move_servo(chip, SERVO_PINS[i], 90);
            }
            else
            {
                printf("서보 %d OFF\n", i + 1);
                move_servo(chip, SERVO_PINS[i], 0);
            }

            usleep(50000); // 간섭 방지
        }

        printf("---------------\n");
        sleep(1);
    }

    gpiod_chip_close(chip);
    return 0;
}
