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

struct gpiod_chip *chip = NULL; // 전역으로 선언

const int TRIG_PINS[SENSOR_COUNT] = {17, 27, 23, 5};   // Physical Pins: 11, 13, 16, 29
const int ECHO_PINS[SENSOR_COUNT] = {4, 22, 24, 6};    // Physical Pins: 7, 15, 18, 31
const int SERVO_PINS[SENSOR_COUNT] = {12, 13, 19, 26}; // Physical Pins: 32, 33, 35, 37

// sensor.c 안에서 실행될 함수
void init_gpio_chip()
{
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip)
    {
        fprintf(stderr, "❌ chip 초기화 실패\n");
        exit(1);
    }
}

long get_microseconds()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
}

int move_distance(struct gpiod_chip *chip, int sensor_index, float *last_distance)
{
    if (sensor_index < 0 || sensor_index >= SENSOR_COUNT)
    {
        fprintf(stderr, "❌ 잘못된 센서 인덱스: %d\n", sensor_index);
        return 0;
    }

    int trig_pin = TRIG_PINS[sensor_index];
    int echo_pin = ECHO_PINS[sensor_index];

    struct gpiod_line *trig = gpiod_chip_get_line(chip, trig_pin);
    struct gpiod_line *echo = gpiod_chip_get_line(chip, echo_pin);

    if (!trig || !echo)
    {
        fprintf(stderr, "❌ 핀 초기화 실패 (Trig: %d, Echo: %d)\n", trig_pin, echo_pin);
        return 0;
    }

    gpiod_line_request_output(trig, "trig", 0);
    gpiod_line_request_input(echo, "echo");

    long start_loop_time = get_microseconds();
    long max_duration = 10000000; // 10초 = 10,000,000µs

    while (get_microseconds() - start_loop_time < max_duration)
    {
        // 트리거 펄스
        gpiod_line_set_value(trig, 0);
        usleep(2);
        gpiod_line_set_value(trig, 1);
        usleep(10);
        gpiod_line_set_value(trig, 0);

        long start_time = 0, end_time = 0;

        // Echo HIGH 대기 (30ms)
        long timeout = get_microseconds() + 30000;
        while (1)
        {
            int val = gpiod_line_get_value(echo);
            if (val < 0)
                break;
            if (val == 1)
            {
                start_time = get_microseconds();
                break;
            }
            if (get_microseconds() > timeout)
                break;
        }

        // Echo LOW 대기 (30ms)
        timeout = get_microseconds() + 30000;
        while (1)
        {
            int val = gpiod_line_get_value(echo);
            if (val < 0)
                break;
            if (val == 0)
            {
                end_time = get_microseconds();
                break;
            }
            if (get_microseconds() > timeout)
                break;
        }

        long duration = end_time - start_time;
        float dist = duration * 0.0343 / 2.0;
        float diff = fabs(dist - *last_distance);
        *last_distance = dist;

        printf("센서 %d 거리: %.2f cm (변화 %.2fcm)\n", sensor_index + 1, dist, diff);

        if (dist <= 10.0 && diff >= 3.0)
        {
            printf("✅ 물품이 들어왔습니다.\n");
            gpiod_line_release(trig);
            gpiod_line_release(echo);
            return 1;
        }

        usleep(500000); // 0.5초 간격 반복
    }

    printf("🔕 10초 동안 물품이 들어오지 않았습니다.\n");
    gpiod_line_release(trig);
    gpiod_line_release(echo);
    return 0;
}

// 서보모터 임시로 구역 02의 서보모터1만 돌아가게 해놓음
void move_servo(struct gpiod_chip *chip, int servo_index)
{
    int servo_pin = SERVO_PINS[servo_index];
    struct gpiod_line *servo = gpiod_chip_get_line(chip, servo_pin);
    if (!servo)
    {
        fprintf(stderr, "❌ 서보 핀 %d 가져오기 실패\n", servo_pin);
        return;
    }

    gpiod_line_request_output(servo, "servo", 0);

    int pulseWidth = 500 + 90 * 11; // 90도
    int cycles = 20;

    // 수정해야함!!! B차가 아직 구역함에서 나갔다는 통신이 구현이 없어서
    //  임의로 서보머터를 열었다가 다시 3초후에 닫히는걸로 했는데
    //  나중엔 B차가 구역함에서 나갔다는 통신이 오면 그때 닫히게 해야함
    //  90도로 이동
    for (int i = 0; i < cycles; i++)
    {
        gpiod_line_set_value(servo, 1);
        usleep(pulseWidth);
        gpiod_line_set_value(servo, 0);
        usleep(20000 - pulseWidth);
    }

    printf("[서보 %d] → 90° 회전\n", servo_index + 1);

    // 3초 대기
    sleep(3);

    // 0도로 복귀
    int resetPulseWidth = 500 + 0 * 11;
    for (int i = 0; i < cycles; i++)
    {
        gpiod_line_set_value(servo, 1);
        usleep(resetPulseWidth);
        gpiod_line_set_value(servo, 0);
        usleep(20000 - resetPulseWidth);
    }

    printf("[서보 %d] → 0° 복귀\n", servo_index + 1);

    gpiod_line_release(servo);
}

// int main()
// {
//     float last_distance = 0.0;

//     chip = gpiod_chip_open_by_name(CHIP_NAME);
//     if (!chip)
//     {
//         fprintf(stderr, "❌ GPIO 칩 열기 실패\n");
//         return 1;
//     }

//     printf("▶️ 무한 루프 시작 (종료하려면 Ctrl+C)\n");

//     while (1)
//     {
//         int triggered = move_distance(chip, 0, &last_distance);

//         if (triggered)
//         {
//             move_servo(chip, 0);
//         }

//         usleep(500000); // 0.5초 딜레이
//     }

//     // 이 코드는 실제로 실행되지 않음 (루프 무한)
//     gpiod_chip_close(chip);
//     return 0;
//}
