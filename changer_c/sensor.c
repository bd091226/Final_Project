#define _GNU_SOURCE #이거 꼭 위에 둬야 함
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define TRIG_PIN 23
#define ECHO_PIN 24
#define SERVO_PIN 12
#define CHIP_NAME "gpiochip0"

long get_microseconds()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
}

float measure_distance(struct gpiod_chip *chip)
{
    struct gpiod_line *trig = gpiod_chip_get_line(chip, TRIG_PIN);
    struct gpiod_line *echo = gpiod_chip_get_line(chip, ECHO_PIN);

    gpiod_line_request_output(trig, "trig", 0);
    gpiod_line_request_input(echo, "echo");

    // Send pulse
    gpiod_line_set_value(trig, 0);
    usleep(2);
    gpiod_line_set_value(trig, 1);
    usleep(10);
    gpiod_line_set_value(trig, 0);

    // Wait for echo
    long start = get_microseconds();
    long timeout = start + 30000;

    while (gpiod_line_get_value(echo) == 0 && get_microseconds() < timeout)
        ;
    long pulse_start = get_microseconds();
    while (gpiod_line_get_value(echo) == 1 && get_microseconds() < timeout)
        ;
    long pulse_end = get_microseconds();

    gpiod_line_release(trig);
    gpiod_line_release(echo);

    long duration = pulse_end - pulse_start;
    float distance = duration / 58.0f;

    return distance;
}

void move_servo(struct gpiod_chip *chip, int angle)
{
    struct gpiod_line *servo = gpiod_chip_get_line(chip, SERVO_PIN);
    gpiod_line_request_output(servo, "servo", 0);

    int pulseWidth = 500 + angle * 11; // 마이크로초
    int cycles = 20;                   // 20번 정도 반복

    for (int i = 0; i < cycles; i++)
    {
        gpiod_line_set_value(servo, 1);
        usleep(pulseWidth);
        gpiod_line_set_value(servo, 0);
        usleep(20000 - pulseWidth);
    }

    gpiod_line_release(servo);
    printf("[서보] 각도: %d°, 펄스: %dus\n", angle, pulseWidth);
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
        float distance = measure_distance(chip);
        printf("거리: %.2f cm\n", distance);

        if (distance < 20.0)
        {
            move_servo(chip, 90);
        }
        else
        {
            move_servo(chip, 0);
        }

        sleep(1);
    }

    gpiod_chip_close(chip);
    return 0;
}
