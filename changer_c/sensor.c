#define _GNU_SOURCE
#include "sensor.h"
#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>

static const int TRIG_PINS[SENSOR_COUNT]  = {
    17, // phys 11 - Sensor 1 TRIG
    27, // phys 13 - Sensor 2 TRIG
    23, // phys 16 - Sensor 3 TRIG
     5  // phys 29 - Sensor 4 TRIG
};

static const int ECHO_PINS[SENSOR_COUNT]  = {
     4, // phys  7 - Sensor 1 ECHO
    22, // phys 15 - Sensor 2 ECHO
    24, // phys 18 - Sensor 3 ECHO
    21  // phys 40 - Sensor 4 ECHO
};

static const int SERVO_PINS[SENSOR_COUNT] = {
    12, // phys 32 - Sensor 1 SERVO
    13, // phys 33 - Sensor 2 SERVO
    18, // phys 12 - Sensor 3 SERVO
    19  // phys 35 - Sensor 4 SERVO
};

static struct gpiod_chip  *chip;
static struct gpiod_line  *trig_lines[SENSOR_COUNT];
static struct gpiod_line  *echo_lines[SENSOR_COUNT];
static struct gpiod_line  *servo_lines[SENSOR_COUNT];
static float               last_distances[SENSOR_COUNT];

// 마이크로초 타이머
static long get_us(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec*1000000L + ts.tv_nsec/1000L;
}

// 내부: 단일 센서 Echo 측정
static void measure_echo_once(int idx, float *dist) {
    long timeout = get_us() + 100000;
    long start, end;
    // Rising edge
    while (gpiod_line_get_value(echo_lines[idx]) == 0) {
        if (get_us() > timeout) { *dist = -1.0f; return; }
    }
    start = get_us();
    // Falling edge
    timeout = get_us() + 100000;
    while (gpiod_line_get_value(echo_lines[idx]) == 1) {
        if (get_us() > timeout) { *dist = -1.0f; return; }
    }
    end = get_us();
    float d = (end - start) * 0.0343f / 2.0f;
    *dist = (d < 2.0f || d > 400.0f) ? -1.0f : d;
}

// 내부: 단일 서보 90°→0°
static void servo_once(int idx) {
    int p90 = 500 + 90*11, p0 = 500, cycles = 20;
    // 90°
    for (int i = 0; i < cycles; i++) {
        gpiod_line_set_value(servo_lines[idx], 1); usleep(p90);
        gpiod_line_set_value(servo_lines[idx], 0); usleep(20000 - p90);
    }
    sleep(3);
    // 0°
    for (int i = 0; i < cycles; i++) {
        gpiod_line_set_value(servo_lines[idx], 1); usleep(p0);
        gpiod_line_set_value(servo_lines[idx], 0); usleep(20000 - p0);
    }
}

// 외부: 초기화 (GPIO 요청 등)
void sensor_init(void) {
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip) { perror("sensor_init"); exit(1); }
    for (int i = 0; i < SENSOR_COUNT; i++) {
        trig_lines[i]  = gpiod_chip_get_line(chip, TRIG_PINS[i]);
        echo_lines[i]  = gpiod_chip_get_line(chip, ECHO_PINS[i]);
        servo_lines[i] = gpiod_chip_get_line(chip, SERVO_PINS[i]);
        if (!trig_lines[i] || !echo_lines[i] || !servo_lines[i]) {
            fprintf(stderr, "sensor_init: line_get 실패 idx=%d\n", i);
            exit(1);
        }
        gpiod_line_request_output(trig_lines[i], "trig", 0);
        gpiod_line_request_input_flags(
            echo_lines[i], "echo",
            GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_DOWN
        );
        gpiod_line_request_output(servo_lines[i], "servo", 0);
        last_distances[i] = -1.0f;
    }
}

// 외부: 4개 센서 거리만 따로 측정
void sensor_read_distances(float distances[SENSOR_COUNT]) {
    for (int i = 0; i < SENSOR_COUNT; i++) {
        // Trig 펄스
        gpiod_line_set_value(trig_lines[i], 0);
        usleep(2);
        gpiod_line_set_value(trig_lines[i], 1);
        usleep(10);
        gpiod_line_set_value(trig_lines[i], 0);
        // Echo 측정
        measure_echo_once(i, &distances[i]);
        usleep(50000); // 센서 간 50ms
    }
}

// 외부: idx 번째 서보만 동작
void sensor_activate_servo(int idx) {
    if (idx < 0 || idx >= SENSOR_COUNT) return;
    servo_once(idx);
}

// 외부: 기존 한 사이클(측정+자동제어)
void sensor_cycle(void) {
    float dist;
    for (int i = 0; i < SENSOR_COUNT; i++) {
        // Trig
        gpiod_line_set_value(trig_lines[i], 0);
        usleep(2);
        gpiod_line_set_value(trig_lines[i], 1);
        usleep(10);
        gpiod_line_set_value(trig_lines[i], 0);
        // Echo
        measure_echo_once(i, &dist);
        // 자동 서보
        if (dist >= 0.0f) {
            float diff = (last_distances[i] < 0.0f
                          ? dist
                          : fabs(dist - last_distances[i]));
            if (dist <= 10.0f && diff >= 2.0f) {
                servo_once(i);
            }
            last_distances[i] = dist;
        }
        usleep(50000);
    }
    usleep(200000);
}

// 외부: 정리
void sensor_cleanup(void) {
    if (chip) {
        gpiod_chip_close(chip);
        chip = NULL;
    }
}

void sensor_monitor_triggers(float dist_thresh,
                             float diff_thresh,
                             unsigned int loop_delay_us,
                             volatile bool *run_flag)
{
    // 이전 거리 저장용 배열
    float prev[SENSOR_COUNT];
    for (int i = 0; i < SENSOR_COUNT; i++) {
        prev[i] = -1.0f;
    }

    // 메인 초기화 검증
    if (!chip) {
        fprintf(stderr, "sensor_monitor_triggers: sensor_init() 먼저 호출하세요\n");
        return;
    }

    // 모니터링 루프
    while (*run_flag) {
        float dists[SENSOR_COUNT];
        sensor_read_distances(dists);

        for (int i = 0; i < SENSOR_COUNT; i++) {
            float d = dists[i];
            if (d < 0.0f) continue;  // 측정 실패 무시

            float diff = (prev[i] < 0.0f)
                         ? 0.0f
                         : fabs(d - prev[i]);

            if (d <= dist_thresh && diff >= diff_thresh) {
                printf("[센서 %d] 거리=%.2fcm Δ=%.2fcm → 조건 충족! 보관함 수량 증가\n",
                       i+1, d, diff);
                fflush(stdout);
            }
            prev[i] = d;
        }

        usleep(loop_delay_us);
    }
}
