// testencoder_nopthread.c
// gcc -g testencoder_nopthread.c -o testencoder_nopthread -lgpiod

#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <sys/poll.h>
#include <time.h>
#include <stdbool.h>
#include "encoder_c.h"

#define SPEED 100


volatile int running = 1;
int countA = 0, countB = 0;


struct gpiod_chip *chip = NULL;
struct gpiod_line *ena = NULL;
struct gpiod_line *enb = NULL;
struct gpiod_line *in1 = NULL;
struct gpiod_line *in2 = NULL;
struct gpiod_line *in3 = NULL;
struct gpiod_line *in4 = NULL;
struct gpiod_line *line1 = NULL;
struct gpiod_line *line2 = NULL;
struct gpiod_line *line3 = NULL;
struct gpiod_line *encA, *encB;
int fdA, fdB;

int init_gpio(void) {
    chip = gpiod_chip_open_by_name(GPIO_CHIP);
    if (!chip) {
        perror("Failed to open gpiochip");
        return -1;
    }

    ena = gpiod_chip_get_line(chip, ENA_PIN);
    enb = gpiod_chip_get_line(chip, ENB_PIN);

    in1 = gpiod_chip_get_line(chip, IN1_PIN);
    in2 = gpiod_chip_get_line(chip, IN2_PIN);
    in3 = gpiod_chip_get_line(chip, IN3_PIN);
    in4 = gpiod_chip_get_line(chip, IN4_PIN);

    line_m1 = gpiod_chip_get_line(chip, MOTOR_IN1);
    line_m2 = gpiod_chip_get_line(chip, MOTOR_IN2);

    line_btn = gpiod_chip_get_line(chip, BUTTON_PIN);

    line1 = gpiod_chip_get_line(chip, GPIO_LINE1);
    line2 = gpiod_chip_get_line(chip, GPIO_LINE2);
    line3 = gpiod_chip_get_line(chip, GPIO_LINE3);

    if (!ena || !enb || !in1 || !in2 || !in3 || !in4 ||
        !line_m1 || !line_m2 || !line_btn || !line1 || !line2 || !line3) {
        perror("Failed to get GPIO lines");
        gpiod_chip_close(chip);
        return -1;
    }

    if (gpiod_line_request_output(ena, "ENA", 1) < 0 ||
        gpiod_line_request_output(enb, "ENB", 1) < 0 ||
        gpiod_line_request_output(in1, "IN1", 0) < 0 ||
        gpiod_line_request_output(in2, "IN2", 0) < 0 ||
        gpiod_line_request_output(in3, "IN3", 0) < 0 ||
        gpiod_line_request_output(in4, "IN4", 0) < 0 ||
        gpiod_line_request_output(line_m1, "motor_ctrl", 0) < 0 ||
        gpiod_line_request_output(line_m2, "motor_ctrl", 0) < 0 ||
        gpiod_line_request_output(line1, "led_ctrl", 0) < 0 ||
        gpiod_line_request_output(line2, "led_ctrl", 0) < 0 ||
        gpiod_line_request_output(line3, "led_ctrl", 0) < 0 ||
        gpiod_line_request_input(line_btn, "btn_read") < 0) {
        perror("Failed to request GPIO line directions");
        gpiod_chip_close(chip);
        return -1;
    }

    // 초기 LED, 모터 상태 설정 (필요하면)

    return 0;
}


void cleanup_and_exit() {
    motor_stop();
    if (ena) gpiod_line_release(ena);
    if (enb) gpiod_line_release(enb);
    if (in1) gpiod_line_release(in1);
    if (in2) gpiod_line_release(in2);
    if (in3) gpiod_line_release(in3);
    if (in4) gpiod_line_release(in4);
    if (encA) gpiod_line_release(encA);
    if (encB) gpiod_line_release(encB);
    if (chip) gpiod_chip_close(chip);
    printf("\n[정리] 종료\n");
    exit(0);
}

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
    cleanup_and_exit();
}

void safe_set_value(struct gpiod_line *line, int value, const char* name) {
    if (!line) {
        fprintf(stderr, "[NULL 라인] %s\n", name);
        cleanup_and_exit();
    }
    if (gpiod_line_set_value(line, value) < 0) {
        fprintf(stderr, "[에러] %s 값 설정 실패\n", name);
        cleanup_and_exit();
    }
}

void motor_stop() {
    safe_set_value(in1, 0, "IN1");
    safe_set_value(in2, 0, "IN2");
    safe_set_value(in3, 0, "IN3");
    safe_set_value(in4, 0, "IN4");
    safe_set_value(ena, 0, "ENA");
    safe_set_value(enb, 0, "ENB");
}

void motor_go(struct gpiod_chip *chip, int speed, double total_duration){
    double moved = 0.0;
    const double check_interval = 0.05;

    safe_set_value(in1, 0, "IN1");
    safe_set_value(in2, 1, "IN2");
    safe_set_value(in3, 0, "IN3");
    safe_set_value(in4, 1, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");

    while (moved < total_duration) {
        usleep(SEC_TO_US(check_interval));
        moved += check_interval;
    }

    motor_stop();
    printf("✅ %.2f초 이동 완료\n", total_duration);
}

void motor_left(double duration) {
    safe_set_value(in1, 1, "IN1");
    safe_set_value(in2, 0, "IN2");
    safe_set_value(in3, 0, "IN3");
    safe_set_value(in4, 1, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");
    usleep(SEC_TO_US(duration));
    motor_stop();
}

void motor_right(double duration) {
    safe_set_value(in1, 0, "IN1");
    safe_set_value(in2, 1, "IN2");
    safe_set_value(in3, 1, "IN3");
    safe_set_value(in4, 0, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");
    usleep(SEC_TO_US(duration));
    motor_stop();
}

void forward_one(Point *pos, int dir) {
    printf("➡️ forward_one at (%d,%d), dir=%d\n", pos->r, pos->c, dir);
    motor_go(chip, 80, 2.10);
    switch (dir) {
        case NORTH: pos->r--; break;
        case EAST:  pos->c++; break;
        case SOUTH: pos->r++; break;
        case WEST:  pos->c--; break;
    }
}

void rotate_one(int *dir, int turn_dir) {
    //motor_go(chip, 80, 0.2);
    usleep(100000);
    if (turn_dir > 0)
        motor_right(SECONDS_PER_90_DEG_ROTATION);
    else 
        motor_left(SECONDS_PER_90_DEG_ROTATION);
    *dir = (*dir + turn_dir + 4) % 4;
}

// --- 시간 기반 모터 제어 함수들 ---
// 아루코 마커 보정시 사용 함수
// 전진
void aruco_forward_time(float sec) {
    safe_set_value(in1, 0, "IN1");
    safe_set_value(in2, 1, "IN2");
    safe_set_value(in3, 0, "IN3");
    safe_set_value(in4, 1, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");

    usleep(SEC_TO_US(sec));
    motor_stop();
}

// 후진
void aruco_backward_time(float sec) {
    safe_set_value(in1, 1, "IN1");
    safe_set_value(in2, 0, "IN2");
    safe_set_value(in3, 1, "IN3");
    safe_set_value(in4, 0, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");

    usleep(SEC_TO_US(sec));
    motor_stop();
}

// 왼쪽 평행 이동 (좌우 중심 보정)
void aruco_left_time(float sec) {
    safe_set_value(in1, 1, "IN1");
    safe_set_value(in2, 0, "IN2");
    safe_set_value(in3, 0, "IN3");
    safe_set_value(in4, 1, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");

    usleep(SEC_TO_US(sec));
    motor_stop();
}

// 오른쪽 평행 이동 (좌우 중심 보정)
void aruco_right_time(float sec) {
    safe_set_value(in1, 0, "IN1");
    safe_set_value(in2, 1, "IN2");
    safe_set_value(in3, 1, "IN3");
    safe_set_value(in4, 0, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");

    usleep(SEC_TO_US(sec));
    motor_stop();
}

// 좌회전 (Yaw 보정)
void rotate_left_time(float sec) {
    safe_set_value(in1, 1, "IN1");
    safe_set_value(in2, 0, "IN2");
    safe_set_value(in3, 1, "IN3");
    safe_set_value(in4, 0, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");

    usleep(SEC_TO_US(sec));
    motor_stop();
}

// 우회전 (Yaw 보정)
void rotate_right_time(float sec) {
    safe_set_value(in1, 0, "IN1");
    safe_set_value(in2, 1, "IN2");
    safe_set_value(in3, 0, "IN3");
    safe_set_value(in4, 1, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");

    usleep(SEC_TO_US(sec));
    motor_stop();
}

void reset_counts() {
    countA = 0;
    countB = 0;
}

void print_counts(const char* tag) {
    printf("[%s] 엔코더 A: %d, B: %d\n", tag, countA, countB);
}

void handle_encoder_events() {
    struct pollfd pfds[2] = {
        { .fd = fdA, .events = POLLIN },
        { .fd = fdB, .events = POLLIN }
    };
    struct gpiod_line_event event;
    int ret = poll(pfds, 2, 10); // 10ms polling

    if (ret > 0) {
        if (pfds[0].revents & POLLIN) {
            gpiod_line_event_read(encA, &event);
            countA++;
        }
        if (pfds[1].revents & POLLIN) {
            gpiod_line_event_read(encB, &event);
            countB++;
        }
    }
}