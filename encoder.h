#ifndef MOTOR_ENCODER_H
#define MOTOR_ENCODER_H

#include <gpiod.h>
#include <sys/poll.h> 
#include <fcntl.h>
#include "Bcar_moter.h"

#define CHIP "gpiochip0"
// BCM GPIO 번호 정의 (libgpiod는 gpiochip0 기준 라인번호 = BCM 번호)
#define IN1_PIN 17
#define IN2_PIN 18
#define ENA_PIN 12
#define IN3_PIN 22
#define IN4_PIN 23
#define ENB_PIN 13
#define SERVO_PIN 26 // 서보 모터 핀
#define BUTTON_PIN 27

// 초음파 센서 핀 (예시, 필요시 변경)
#define TRIG_PIN 6
#define ECHO_PIN 5
#define ENCA 14
#define ENCB 15

#define NORTH 0
#define EAST  1
#define SOUTH 2
#define WEST  3

#define PWM_PERIOD_US 2000  // PWM 주기: 2000µs → 500Hz

#define CONTROL_INTERVAL_MS   100   // 제어 루프 주기 (ms)
#define KP   0.8                  // PID 비례 이득
#define KI   0.2                  // PID 적분 이득
#define KD   0.05                 // PID 미분 이득
#define DUTY_MIN   0              // PWM 듀티 최소 [%]
#define DUTY_MAX   100            // PWM 듀티 최대 [%]

// forward_one 에서 사용할 전진 속도·시간
#define FORWARD_SPEED      80     // [%]
#define FORWARD_SEC        0.86    // grid 한 칸 당 시간 (s)

// rotate_one 에서 사용할 예비 전진 및 회전 속도·시간
#define ROTATE_SPEED       100     // [%]
#define ROTATE_PRE_FWD_SEC 0.1    // 회전 전 예비 전진 시간 (s)
#define ROTATE_90_SEC      0.62   // 90° 회전 시간 (s)

// 100% 속도일 때 100ms 동안 기대되는 엔코더 A 카운트 (실험으로 결정)
#define NOMINAL_PULSES_PER_INTERVAL 50

extern struct gpiod_chip *chip;
extern struct gpiod_line *ena, *enb, *in1, *in2, *in3, *in4;
extern struct gpiod_line *line_m1, *line_m2, *line_btn;
extern struct gpiod_line *encA, *encB;
extern struct gpiod_line *servo_line;
extern struct gpiod_line *trig_line, *echo_line;

// 외부에서 접근 가능한 변수
extern volatile int running;
extern int countA, countB;
extern int fdA, fdB;
extern struct pollfd pfds[2];
extern struct gpiod_line_event evt;

// 초기화 및 정리
int init_gpio(void);
void test_motor_encoder();
void encoder_poll_init(void);
void handle_sigint(int sig);
void cleanup_and_exit(void);

// 유틸
long get_microseconds(void);
void safe_set_value(struct gpiod_line *line, int value, const char* name);
void motor_stop(void);
void reset_counts(void);
void print_counts(const char* tag);
void handle_encoder_events(void);

// 저수준 모터 제어
void motor_go(struct gpiod_chip *chip, int speed_pct, double duration_s);
void motor_left(struct gpiod_chip *chip, int speed_pct, double duration_s);
void motor_right(struct gpiod_chip *chip, int speed_pct, double duration_s);

// 고수준 이동 API
void forward_one(Point *pos, int dir);
void rotate_one(int *dir, int turn_dir);

// // 함수 선언
// void safe_set_value(struct gpiod_line *line, int value, const char* name);
// long get_microseconds(void);
// void motor_stop(void);
// void motor_go(struct gpiod_chip *chip, int speed, int target_pulses);
// void motor_left(struct gpiod_chip *chip, int speed, int target_pulses);
// void motor_right(struct gpiod_chip *chip, int speed, int target_pulses);
// void forward_one(Point *pos, int dir);
// void rotate_one(int *dir, int turn_dir);
// void encoder_poll_init(void);
// void reset_counts();
// void print_counts(const char* tag);
// void handle_encoder_events();
// void cleanup_and_exit();
// void handle_sigint(int sig);
// int init_gpio();

#endif // MOTOR_ENCODER_H
