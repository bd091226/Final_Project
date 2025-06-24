// encoder.c

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
#include "encoder.h"

#define SPEED 100


volatile int running = 1;
int countA = 0, countB = 0;

// 실제 메모리 공간 정의
struct gpiod_chip *chip = NULL;

struct gpiod_line *ena = NULL;
struct gpiod_line *enb = NULL;
struct gpiod_line *in1 = NULL;
struct gpiod_line *in2 = NULL;
struct gpiod_line *in3 = NULL;
struct gpiod_line *in4 = NULL;
struct gpiod_line *line_btn = NULL;
struct gpiod_line *encA = NULL;
struct gpiod_line *encB = NULL;

struct gpiod_line *servo_line = NULL;
struct gpiod_line *trig_line = NULL;
struct gpiod_line *echo_line = NULL;
int fdA, fdB;

int init_gpio(void) {
    printf("초기화 중...\n");

    chip = gpiod_chip_open_by_name(GPIO_CHIP);
    if (!chip) {
        perror("Failed to open gpiochip");
        return -1;
    }

    // 모터 제어용 라인 가져오기
    ena = gpiod_chip_get_line(chip, ENA_PIN);
    enb = gpiod_chip_get_line(chip, ENB_PIN);
    in1 = gpiod_chip_get_line(chip, IN1_PIN);
    in2 = gpiod_chip_get_line(chip, IN2_PIN);
    in3 = gpiod_chip_get_line(chip, IN3_PIN);
    in4 = gpiod_chip_get_line(chip, IN4_PIN);

    // 버튼 입력 라인
    line_btn = gpiod_chip_get_line(chip, BUTTON_PIN);

    // 엔코더 라인
    encA = gpiod_chip_get_line(chip, ENCA);
    encB = gpiod_chip_get_line(chip, ENCB);

    if (!ena || !enb || !in1 || !in2 || !in3 || !in4) {
        perror("모터 제어용 GPIO 라인 가져오기 실패");
        gpiod_chip_close(chip);
        return -1;
    }

    if (!line_btn) {
        perror("버튼 GPIO 라인 가져오기 실패");
        gpiod_chip_close(chip);
        return -1;
    }

    if (!encA || !encB) {
        perror("엔코더 GPIO 라인 가져오기 실패");
        gpiod_chip_close(chip);
        return -1;
    }

    // GPIO 출력 요청 (모터 제어)
    if (gpiod_line_request_output(ena, "ENA", 1) < 0 ||
        gpiod_line_request_output(enb, "ENB", 1) < 0 ||
        gpiod_line_request_output(in1, "IN1", 0) < 0 ||
        gpiod_line_request_output(in2, "IN2", 0) < 0 ||
        gpiod_line_request_output(in3, "IN3", 0) < 0 ||
        gpiod_line_request_output(in4, "IN4", 0) < 0) {
        perror("모터 제어용 GPIO 요청 실패");
        gpiod_chip_close(chip);
        return -1;
    }

    // 버튼 입력 요청
    if (gpiod_line_request_input(line_btn, "btn_read") < 0) {
        perror("버튼 GPIO 입력 요청 실패");
        gpiod_chip_close(chip);
        return -1;
    }

    // 엔코더 요청
    if (gpiod_line_request_both_edges_events_flags(
            encA, "encA",
            GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP) < 0) {
        perror("encA 요청 실패");
        return -1;
    }
    if (gpiod_line_request_both_edges_events_flags(
            encB, "encB",
            GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP) < 0) {
        perror("encB 요청 실패");
        return -1;
    }

    fdA = gpiod_line_event_get_fd(encA);
    fdB = gpiod_line_event_get_fd(encB);
    printf("fdA = %d, fdB = %d\n", fdA, fdB);
    
    printf("GPIO 초기화 완료\n");
    return 0;
}

// 현재 시각을 마이크로초 단위로 반환 (초음파 측정용)
long get_microseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
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

    struct pollfd pfds[2] = {
        { .fd = fdA, .events = POLLIN },
        { .fd = fdB, .events = POLLIN }
    };
    struct gpiod_line_event evt;
    
     // 3) 타이머 계산
     long start_us = get_microseconds();
     long end_us   = start_us + (long)(total_duration * 1e6);
 
     // 4) poll 기반 블록킹 루프
     while (get_microseconds() < end_us) {
         int ret = poll(pfds, 2, 100);  // 100ms 타임아웃
         if (ret < 0) {
             perror("poll error");
             break;
         }
         if (ret == 0) {
             continue;
         }
         if (pfds[0].revents & POLLIN) {
             gpiod_line_event_read(encA, &evt);
             countA++;
         }
         if (pfds[1].revents & POLLIN) {
             gpiod_line_event_read(encB, &evt);
             countB++;
         }
     } 

    motor_stop();
    print_counts("motor_go");
    printf("✅ %.2f초 이동 완료\n", total_duration);
}

void motor_left(double duration) {
    safe_set_value(in1, 1, "IN1");
    safe_set_value(in2, 0, "IN2");
    safe_set_value(in3, 0, "IN3");
    safe_set_value(in4, 1, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");
    
    struct pollfd pfds[2] = {
        { .fd = fdA, .events = POLLIN },
        { .fd = fdB, .events = POLLIN }
    };
    struct gpiod_line_event evt;

    // 3) 타이머 계산
    long start_us = get_microseconds();
    long end_us   = start_us + (long)(duration * 1e6);

    // 4) poll 기반 블록킹 루프
    while (get_microseconds() < end_us) {
        int ret = poll(pfds, 2, 100);
        if (ret < 0) {
            perror("poll error");
            break;
        }
        if (ret == 0) {
            continue;
        }
        if (pfds[0].revents & POLLIN) {
            gpiod_line_event_read(encA, &evt);
            countA++;
        }
        if (pfds[1].revents & POLLIN) {
            gpiod_line_event_read(encB, &evt);
            countB++;
        }
    }

    motor_stop();
    print_counts("motor_left");
}

void motor_right(double duration) {
    safe_set_value(in1, 0, "IN1");
    safe_set_value(in2, 1, "IN2");
    safe_set_value(in3, 1, "IN3");
    safe_set_value(in4, 0, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");

    struct pollfd pfds[2] = {
        { .fd = fdA, .events = POLLIN },
        { .fd = fdB, .events = POLLIN }
    };
    struct gpiod_line_event evt;

    // 3) 타이머 계산
    long start_us = get_microseconds();
    long end_us   = start_us + (long)(duration * 1e6);

    // 4) poll 기반 블록킹 루프
    while (get_microseconds() < end_us) {
        int ret = poll(pfds, 2, 100);
        if (ret < 0) {
            perror("poll error");
            break;
        }
        if (ret == 0) {
            continue;
        }
        if (pfds[0].revents & POLLIN) {
            gpiod_line_event_read(encA, &evt);
            countA++;
        }
        if (pfds[1].revents & POLLIN) {
            gpiod_line_event_read(encB, &evt);
            countB++;
        }
    }

    motor_stop();
    print_counts("motor_right");
}

void forward_one(Point *pos, int dir) {
    printf("➡️ forward_one at (%d,%d), dir=%d\n", pos->r, pos->c, dir);
    motor_go(chip, SPEED, 3.0);
    switch (dir) {
        case NORTH: pos->r--; break;
        case EAST:  pos->c++; break;
        case SOUTH: pos->r++; break;
        case WEST:  pos->c--; break;
    }
}

void rotate_one(int *dir, int turn_dir) {
    double t0 = (PRE_ROTATE_FORWARD_CM / 30.0) * SECONDS_PER_GRID_STEP;
    motor_go(chip, SPEED, t0);
    usleep(100000);
    if (turn_dir > 0)
        motor_right(SECONDS_PER_90_DEG_ROTATION);
    else 
        motor_left(SECONDS_PER_90_DEG_ROTATION);
    *dir = (*dir + turn_dir + 4) % 4;
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
    int ret = poll(pfds, 2, 0);

    if (ret > 0) {
        if (pfds[0].revents & POLLIN) {
            gpiod_line_event_read(encA, &event);
            countA++;
            printf("encA 이벤트 발생: countA = %d\n", countA);
        }
        if (pfds[1].revents & POLLIN) {
            gpiod_line_event_read(encB, &event);
            countB++;
            printf("encB 이벤트 발생: countB = %d\n", countB);
        }
    }    
}