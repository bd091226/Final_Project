#ifndef MOTOR_ENCODER_H
#define MOTOR_ENCODER_H

#include <gpiod.h>
#include <stdbool.h>
#include "Bcar_moter.h"

#define GPIO_CHIP "gpiochip0"
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

extern struct gpiod_chip *chip;
extern struct gpiod_line *ena, *enb, *in1, *in2, *in3, *in4;
extern struct gpiod_line *line_m1, *line_m2, *line_btn;
extern struct gpiod_line *encA, *encB;
// 외부에서 접근 가능한 변수
extern volatile int running;
extern int countA, countB;
int init_gpio();
extern int fdA;
extern int fdB;
// 함수 선언
void safe_set_value(struct gpiod_line *line, int value, const char* name);
long get_microseconds(void);
void motor_stop();
void motor_go(struct gpiod_chip *chip, int speed, double total_duration);
void motor_left(double duration);
void motor_right(double duration);
void forward_one(Point *pos, int dir);
void rotate_one(int *dir, int turn_dir);
void reset_counts();
void print_counts(const char* tag);
void handle_encoder_events();
void cleanup_and_exit();
void handle_sigint(int sig);
int init_gpio();

#endif // MOTOR_ENCODER_H
