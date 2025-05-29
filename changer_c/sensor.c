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

const int TRIG_PINS[SENSOR_COUNT] = {17, 27, 23, 5};
const int ECHO_PINS[SENSOR_COUNT] = {4, 22, 24, 6};
const int SERVO_PINS[SENSOR_COUNT] = {12, 13, 19, 26};

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

    float dist = measure_distance_by_index(chip, sensor_index);
    float diff = fabs(dist - *last_distance);
    *last_distance = dist; // 이전 거리 갱신

    printf("센서 %d 거리: %.2f cm → ", sensor_index + 1, dist);

    if (dist <= 15.0 && diff >= 5.0)
    {
        printf("✅ 물품이 들어왔습니다. (변화 %.2fcm)\n", diff);
        return 1;
    }
    else
    {
        printf("🔕 물품이 들어오지 않았습니다.\n");
        return 0;
    }
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
