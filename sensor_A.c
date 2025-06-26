#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

#define CHIP_NAME "gpiochip0"

const int TRIG_PIN = 5;
const int ECHO_PIN = 9;

long get_microseconds()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
}

// 초음파 센서 핀 초기화 함수
void init_ultrasonic_pins(struct gpiod_chip *chip) {
    struct gpiod_line *trig = gpiod_chip_get_line(chip, TRIG_PIN);
    struct gpiod_line *echo = gpiod_chip_get_line(chip, ECHO_PIN);

    if (!trig || !echo) {
        perror("gpiod_chip_get_line (ultrasonic)");
        exit(EXIT_FAILURE);
    }

    // Trig 핀 출력 설정 (필수)
    if (gpiod_line_request_output(trig, "ultrasonic_trig", 0) < 0) {
        perror("gpiod_line_request_output (TRIG)");
        exit(EXIT_FAILURE);
    }

    // Echo 핀 입력 설정 (필수)
    if (gpiod_line_request_input(echo, "ultrasonic_echo") < 0) {
        perror("gpiod_line_request_input (ECHO)");
        exit(EXIT_FAILURE);
    }

    // 핀 해제
    gpiod_line_release(trig);
    gpiod_line_release(echo);
}

float measure_distance(struct gpiod_chip *chip)
{
    struct gpiod_line *trig = gpiod_chip_get_line(chip, TRIG_PIN);
    struct gpiod_line *echo = gpiod_chip_get_line(chip, ECHO_PIN);

    gpiod_line_request_output(trig, "trig", 0);
    gpiod_line_request_input(echo, "echo");

    // 트리거 펄스
    gpiod_line_set_value(trig, 0);
    usleep(2);
    gpiod_line_set_value(trig, 1);
    usleep(10);
    gpiod_line_set_value(trig, 0);

    long start_time = 0, end_time = 0, current_time;

    // Echo HIGH 대기 (타임아웃 30ms)
    long timeout = get_microseconds() + 30000;
    while (1) {
        current_time = get_microseconds();
        if (current_time > timeout) {
            gpiod_line_release(trig);
            gpiod_line_release(echo);
            return -1;  // 타임아웃 시 -1 반환
        }
        if (gpiod_line_get_value(echo) == 1) {
            start_time = current_time;
            break;
        }
    }

    // Echo LOW 대기 (타임아웃 30ms)
    timeout = get_microseconds() + 30000;
    while (1) {
        current_time = get_microseconds();
        if (current_time > timeout) {
            gpiod_line_release(trig);
            gpiod_line_release(echo);
            return -1;  // 타임아웃 시 -1 반환
        }
        if (gpiod_line_get_value(echo) == 0) {
            end_time = current_time;
            break;
        }
    }

    gpiod_line_release(trig);
    gpiod_line_release(echo);

    float dist = (end_time - start_time) * 0.0343 / 2.0;
    return dist;
}

bool check_obstacle(struct gpiod_chip *chip)
{
    float distance = measure_distance(chip);

    if (distance < 0)
    {
        return false; // 타임아웃 시 장애물로 인식하지 않음
    }

    if (distance <= 8.0 && distance > 0.1)
    {
        printf(" 장애물 감지! 이동 중지! 거리: %.2f cm\n", distance);
        return true;
    }
    
    return false; // 장애물이 없으면 false 반환
}
