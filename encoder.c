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

struct pollfd pfds[2];
struct gpiod_line_event evt;

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

void reset_counts(void) {
    countA = 0;
    countB = 0;
}

void motor_stop() {
    safe_set_value(in1, 0, "IN1");
    safe_set_value(in2, 0, "IN2");
    safe_set_value(in3, 0, "IN3");
    safe_set_value(in4, 0, "IN4");
    safe_set_value(ena, 0, "ENA");
    safe_set_value(enb, 0, "ENB");
}

// void motor_go(struct gpiod_chip *chip, int speed, int target_pulses){
//     (void)chip;  // 현재는 사용 안 함
//     (void)speed; // PWM 제어 시 사용 예정

//     reset_counts();

//     // 1) 전진 핀 세팅
//     gpiod_line_set_value(in1, 0);
//     gpiod_line_set_value(in2, 1);
//     gpiod_line_set_value(in3, 0);
//     gpiod_line_set_value(in4, 1);
//     gpiod_line_set_value(ena, 1);
//     gpiod_line_set_value(enb, 1);

//     // 2) 목표 펄스 도달할 때까지 poll + 이벤트 읽기
//     // while (countA < target_pulses && countB < target_pulses) {
//     //     if (poll(pfds, 2, 100) > 0) {
//     //         if (pfds[0].revents & POLLIN) { 
//     //             gpiod_line_event_read(encA, &evt); 
//     //             countA++; 
//     //         }
//     //         if (pfds[1].revents & POLLIN) { 
//     //             gpiod_line_event_read(encB, &evt); 
//     //             countB++; 
//     //         }
//     //     }
//     // }
//     while (countA < target_pulses) {
//         if (poll(pfds, 2, 100) > 0) {
//             if (pfds[0].revents & POLLIN) {
//                 gpiod_line_event_read(encA, &evt);
//                 countA++;
//             }
//             if (pfds[1].revents & POLLIN) {
//                 gpiod_line_event_read(encB, &evt);
//                 countB++;
//             }
//         }
//     }

//     // 3) 정지 및 결과 출력
//     motor_stop();
//     printf("[GO]     A:%d B:%d\n", countA, countB);
// }

// void motor_left(struct gpiod_chip *chip, int speed, int target_pulses) {
//     (void)chip; (void)speed;
//     reset_counts();

//     // 좌회전 핀 세팅 (CCW)
//     gpiod_line_set_value(in1, 1);
//     gpiod_line_set_value(in2, 0);
//     gpiod_line_set_value(in3, 0);
//     gpiod_line_set_value(in4, 1);
//     gpiod_line_set_value(ena,  1);
//     gpiod_line_set_value(enb,  1);

//     // while (countA < target_pulses && countB < target_pulses) {
//     //     if (poll(pfds, 2, 100) > 0) {
//     //         if (pfds[0].revents & POLLIN) { 
//     //             gpiod_line_event_read(encA, &evt); 
//     //             countA++; }
//     //         if (pfds[1].revents & POLLIN) {
//     //             gpiod_line_event_read(encB, &evt); 
//     //             countB++; }
//     //     }
//     // }

//     while (countA < target_pulses) {
//         if (poll(pfds, 2, 100) > 0) {
//             if (pfds[0].revents & POLLIN) {
//                 gpiod_line_event_read(encA, &evt);
//                 countA++;
//             }
//             if (pfds[1].revents & POLLIN) {
//                 gpiod_line_event_read(encB, &evt);
//                 countB++;
//             }
//         }
//     }
    
//     motor_stop();
//     printf("[LEFT]   A:%d B:%d\n", countA, countB);
// }

// void motor_right(struct gpiod_chip *chip, int speed, int target_pulses) {
//     (void)chip; (void)speed;
//     reset_counts();

//     // 우회전 핀 세팅 (CW)
//     gpiod_line_set_value(in1, 0);
//     gpiod_line_set_value(in2, 1);
//     gpiod_line_set_value(in3, 1);
//     gpiod_line_set_value(in4, 0);
//     gpiod_line_set_value(ena,  1);
//     gpiod_line_set_value(enb,  1);

//     while (countA < target_pulses || countB < target_pulses) {
//         if (poll(pfds, 2, 100) > 0) {
//             if (pfds[0].revents & POLLIN) {
//                 gpiod_line_event_read(encA, &evt);
//                 countA++;
//             }
//             if (pfds[1].revents & POLLIN) {
//                 gpiod_line_event_read(encB, &evt);
//                 countB++;
//             }
//         }
//     }

//     motor_stop();
//     printf("[RIGHT]  A:%d B:%d\n", countA, countB);
// }

// // PWM 기반 좌회전 (CCW)
// void motor_left(struct gpiod_chip *chip_unused, int speed, int target_pulses) {
//     (void)chip_unused;
//     reset_counts();

//     gpiod_line_set_value(in1, 1);
//     gpiod_line_set_value(in2, 0);
//     gpiod_line_set_value(in3, 0);
//     gpiod_line_set_value(in4, 1);

//     int high_us = PWM_PERIOD_US * speed / 100;
//     int low_us  = PWM_PERIOD_US - high_us;

//     while (countA < target_pulses) {
//         gpiod_line_set_value(ena, 1);
//         gpiod_line_set_value(enb, 1);
//         usleep(high_us);

//         if (poll(pfds, 2, 0) > 0 && (pfds[0].revents|pfds[1].revents)) {
//             if (pfds[0].revents & POLLIN) { gpiod_line_event_read(encA,&evt); countA++; }
//             if (pfds[1].revents & POLLIN) { gpiod_line_event_read(encB,&evt); countB++; }
//         }

//         gpiod_line_set_value(ena, 0);
//         gpiod_line_set_value(enb, 0);
//         usleep(low_us);

//         if (poll(pfds, 2, 0) > 0 && (pfds[0].revents|pfds[1].revents)) {
//             if (pfds[0].revents & POLLIN) { gpiod_line_event_read(encA,&evt); countA++; }
//             if (pfds[1].revents & POLLIN) { gpiod_line_event_read(encB,&evt); countB++; }
//         }
//     }

//     motor_stop();
//     printf("[LEFT]   A:%d B:%d @%d%%\n", countA, countB, speed);
// }

// // PWM 기반 우회전 (CW)
// void motor_right(struct gpiod_chip *chip_unused, int speed, int target_pulses) {
//     (void)chip_unused;
//     reset_counts();

//     gpiod_line_set_value(in1, 0);
//     gpiod_line_set_value(in2, 1);
//     gpiod_line_set_value(in3, 1);
//     gpiod_line_set_value(in4, 0);

//     int high_us = PWM_PERIOD_US * speed / 100;
//     int low_us  = PWM_PERIOD_US - high_us;

//     while (countA < target_pulses) {
//         gpiod_line_set_value(ena, 1);
//         gpiod_line_set_value(enb, 1);
//         usleep(high_us);

//         if (poll(pfds, 2, 0) > 0 && (pfds[0].revents|pfds[1].revents)) {
//             if (pfds[0].revents & POLLIN) { gpiod_line_event_read(encA,&evt); countA++; }
//             if (pfds[1].revents & POLLIN) { gpiod_line_event_read(encB,&evt); countB++; }
//         }

//         gpiod_line_set_value(ena, 0);
//         gpiod_line_set_value(enb, 0);
//         usleep(low_us);

//         if (poll(pfds, 2, 0) > 0 && (pfds[0].revents|pfds[1].revents)) {
//             if (pfds[0].revents & POLLIN) { gpiod_line_event_read(encA,&evt); countA++; }
//             if (pfds[1].revents & POLLIN) { gpiod_line_event_read(encB,&evt); countB++; }
//         }
//     }

//     motor_stop();
//     printf("[RIGHT]  A:%d B:%d @%d%%\n", countA, countB, speed);
// }

// 100ms 동안 엔코더 A펄스만 세는 helper
static int count_encoder_A(int interval_us) {
    struct pollfd pfd = { .fd = fdA, .events = POLLIN };
    struct gpiod_line_event e;
    int cnt = 0;
    long start = get_microseconds();
    while (get_microseconds() - start < interval_us) {
        if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN)) {
            gpiod_line_event_read(encA, &e);
            cnt++;
        }
    }
    return cnt;
}

// 내부: PID 루프 기반 드라이브
static void pid_drive(int speed_pct, double duration_s,
                      void (*dir_pins)(void))
{
    double integral = 0, last_err = 0;
    int interval_us = CONTROL_INTERVAL_MS * 1000;
    // 100% 속도일 때 100ms 기대 펄스 → 비례 배분
    int target = speed_pct * NOMINAL_PULSES_PER_INTERVAL / 100;
    long end_t = get_microseconds() + (long)(duration_s * 1e6);

    reset_counts();
    while (get_microseconds() < end_t) {
        // 1) 방향 핀 세팅
        dir_pins();

        // 2) 한 인터벌간 카운트
        int measured = count_encoder_A(interval_us);

        // 3) PID
        double err = target - measured;
        integral += err * (CONTROL_INTERVAL_MS/1000.0);
        double deriv = (err - last_err)/(CONTROL_INTERVAL_MS/1000.0);
        last_err = err;
        double u = KP*err + KI*integral + KD*deriv;
        int duty = (int)u;
        if (duty < 0)   duty = 0;
        if (duty > 100) duty = 100;

        // 4) PWM
        gpiod_line_set_value(ena, 1);
        gpiod_line_set_value(enb, 1);
        usleep(interval_us * duty / 100);
        gpiod_line_set_value(ena, 0);
        gpiod_line_set_value(enb, 0);
        usleep(interval_us * (100-duty) / 100);
    }
    motor_stop();
}

// 방향별 핀 세팅
static void dir_forward(void) {
    gpiod_line_set_value(in1,0); gpiod_line_set_value(in2,1);
    gpiod_line_set_value(in3,0); gpiod_line_set_value(in4,1);
}
static void dir_left(void) {
    gpiod_line_set_value(in1,1); gpiod_line_set_value(in2,0);
    gpiod_line_set_value(in3,0); gpiod_line_set_value(in4,1);
}
static void dir_right(void) {
    gpiod_line_set_value(in1,0); gpiod_line_set_value(in2,1);
    gpiod_line_set_value(in3,1); gpiod_line_set_value(in4,0);
}

// ----------------------------------------------------------------------------
// 전진: speed_pct% 듀티로 duration_s 초 동안 PWM, 인코더 A/B 펄스 카운트
// ----------------------------------------------------------------------------
void motor_go(struct gpiod_chip *chip_unused,
    int speed_pct,
    double duration_s)
{
    (void)chip_unused;
    reset_counts();
    dir_forward();

    // 인코더 fd 준비
    struct pollfd pfds[2] = {
    { .fd = gpiod_line_event_get_fd(encA), .events = POLLIN },
    { .fd = gpiod_line_event_get_fd(encB), .events = POLLIN }
    };
    struct gpiod_line_event evt;

    long end_us = get_microseconds() + (useconds_t)(duration_s * 1e6);

    while (get_microseconds() < end_us) {
    // 1) 듀티비 계산
    int high_us = PWM_PERIOD_US * speed_pct / 100;
    int low_us  = PWM_PERIOD_US - high_us;

    // 2) ON 시간 동안 펄스 측정
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");
    long start = get_microseconds();
    while (get_microseconds() - start < high_us) {
    if (poll(pfds, 2, 0) > 0) {
        if (pfds[0].revents & POLLIN) {
            gpiod_line_event_read(encA, &evt);
            countA++;
        }
        if (pfds[1].revents & POLLIN) {
            gpiod_line_event_read(encB, &evt);
            countB++;
        }
    }
    }

    // 3) OFF 시간 동안 펄스 측정
    safe_set_value(ena, 0, "ENA");
    safe_set_value(enb, 0, "ENB");
    start = get_microseconds();
    while (get_microseconds() - start < low_us) {
    if (poll(pfds, 2, 0) > 0) {
        if (pfds[0].revents & POLLIN) {
            gpiod_line_event_read(encA, &evt);
            countA++;
        }
        if (pfds[1].revents & POLLIN) {
            gpiod_line_event_read(encB, &evt);
            countB++;
        }
    }
    }
    }

    motor_stop();
    printf("[GO]     actual A:%d B:%d @%d%% for %.2fs\n",
    countA, countB, speed_pct, duration_s);
}

// ----------------------------------------------------------------------------
// 제자리 좌회전 (CCW) 동일 방식
// ----------------------------------------------------------------------------
void motor_left(struct gpiod_chip *chip_unused,
      int speed_pct,
      double duration_s)
{
    (void)chip_unused;
    reset_counts();
    dir_left();

    struct pollfd pfds[2] = {
    { .fd = gpiod_line_event_get_fd(encA), .events = POLLIN },
    { .fd = gpiod_line_event_get_fd(encB), .events = POLLIN }
    };
    struct gpiod_line_event evt;

    long end_us = get_microseconds() + (useconds_t)(duration_s * 1e6);

    while (get_microseconds() < end_us) {
    int high_us = PWM_PERIOD_US * speed_pct / 100;
    int low_us  = PWM_PERIOD_US - high_us;

    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");
    long start = get_microseconds();
    while (get_microseconds() - start < high_us) {
    if (poll(pfds, 2, 0) > 0) {
        if (pfds[0].revents & POLLIN) {
            gpiod_line_event_read(encA, &evt);
            countA++;
        }
        if (pfds[1].revents & POLLIN) {
            gpiod_line_event_read(encB, &evt);
            countB++;
        }
    }
    }

    safe_set_value(ena, 0, "ENA");
    safe_set_value(enb, 0, "ENB");
    start = get_microseconds();
    while (get_microseconds() - start < low_us) {
    if (poll(pfds, 2, 0) > 0) {
        if (pfds[0].revents & POLLIN) {
            gpiod_line_event_read(encA, &evt);
            countA++;
        }
        if (pfds[1].revents & POLLIN) {
            gpiod_line_event_read(encB, &evt);
            countB++;
        }
    }
    }
    }

    motor_stop();
    printf("[LEFT]   actual A:%d B:%d @%d%% for %.2fs\n",
    countA, countB, speed_pct, duration_s);
}

// ----------------------------------------------------------------------------
// 제자리 우회전 (CW) 동일 방식
// ----------------------------------------------------------------------------
void motor_right(struct gpiod_chip *chip_unused,
       int speed_pct,
       double duration_s)
{
    (void)chip_unused;
    reset_counts();
    dir_right();

    struct pollfd pfds[2] = {
    { .fd = gpiod_line_event_get_fd(encA), .events = POLLIN },
    { .fd = gpiod_line_event_get_fd(encB), .events = POLLIN }
    };
    struct gpiod_line_event evt;

    long end_us = get_microseconds() + (useconds_t)(duration_s * 1e6);

    while (get_microseconds() < end_us) {
    int high_us = PWM_PERIOD_US * speed_pct / 100;
    int low_us  = PWM_PERIOD_US - high_us;

    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");
    long start = get_microseconds();
    while (get_microseconds() - start < high_us) {
    if (poll(pfds, 2, 0) > 0) {
        if (pfds[0].revents & POLLIN) {
            gpiod_line_event_read(encA, &evt);
            countA++;
        }
        if (pfds[1].revents & POLLIN) {
            gpiod_line_event_read(encB, &evt);
            countB++;
        }
    }
    }

    safe_set_value(ena, 0, "ENA");
    safe_set_value(enb, 0, "ENB");
    start = get_microseconds();
    while (get_microseconds() - start < low_us) {
    if (poll(pfds, 2, 0) > 0) {
        if (pfds[0].revents & POLLIN) {
            gpiod_line_event_read(encA, &evt);
            countA++;
        }
        if (pfds[1].revents & POLLIN) {
            gpiod_line_event_read(encB, &evt);
            countB++;
        }
    }
    }
    }

    motor_stop();
    printf("[RIGHT]  actual A:%d B:%d @%d%% for %.2fs\n",
    countA, countB, speed_pct, duration_s);
}

//————————————————————————————————
//  forward_one, rotate_one (main에서 호출)
//————————————————————————————————
void forward_one(Point *pos, int dir) {
    printf("➡️ forward_one at (%d,%d), dir=%d\n", pos->r, pos->c, dir);
    motor_go(NULL, FORWARD_SPEED, FORWARD_SEC);
    switch(dir) {
      case NORTH: pos->r--; break;
      case EAST : pos->c++; break;
      case SOUTH: pos->r++; break;
      case WEST : pos->c--; break;
    }
}

void rotate_one(int *dir, int turn_dir) {
    // 1) 예비 전진
    motor_go(NULL, ROTATE_SPEED, ROTATE_PRE_FWD_SEC);
    // 2) 회전
    if (turn_dir > 0)
        motor_right(NULL, ROTATE_SPEED, ROTATE_90_SEC);
    else
        motor_left(NULL, ROTATE_SPEED, ROTATE_90_SEC);
    // 3) 방향 갱신
    *dir = (*dir + turn_dir + 4) % 4;
}

