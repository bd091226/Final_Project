#ifndef MOTOR_ENCODER_H
#define MOTOR_ENCODER_H

#include <gpiod.h>
#include <stdbool.h>

// 모터 동작 타이밍
#define SEC_TO_US(sec) ((useconds_t)((sec) * 1e6))
#define SECONDS_PER_GRID_STEP 2.0
#define SECONDS_PER_90_DEG_ROTATION 0.81
#define PRE_ROTATE_FORWARD_CM 6.0
 
#define GPIO_CHIP "/dev/gpiochip0"
#define IN1 22
#define IN2 27
#define ENA 18
#define IN3 25
#define IN4 24
#define ENB 23
#define ENC_A 14
#define ENC_B 15
#define MOTOR_IN1       19  // L298N IN1
#define MOTOR_IN2       20  // L298N IN2
#define BUTTON_PIN      17  // 버튼 입력 핀

#define GPIO_LINE1      12  // 빨강 LED
#define GPIO_LINE2      13  // 하양 LED
#define GPIO_LINE3      6  // 초록 LED

extern struct gpiod_chip *chip;
extern struct gpiod_line *ena, *enb, *in1, *in2, *in3, *in4;
extern struct gpiod_line *line_m1, *line_m2, *line_btn;
extern struct gpiod_line *line1, *line2, *line3;


// PIN 이름도 동일하게 정의
#define IN1_PIN IN1
#define IN2_PIN IN2
#define IN3_PIN IN3
#define IN4_PIN IN4
#define ENA_PIN ENA
#define ENB_PIN ENB

#define NORTH 0
#define EAST  1
#define SOUTH 2
#define WEST  3

extern struct gpiod_chip *chip;
extern struct gpiod_line *ena, *enb;
extern struct gpiod_line *in1, *in2, *in3, *in4;
extern struct gpiod_line *line1, *line2, *line3;
extern struct gpiod_line *encA, *encB;
extern struct gpiod_line *line_m1;  // 컨베이어 모터용
extern struct gpiod_line *line_m2;  // 컨베이어 모터용
extern struct gpiod_line *line_btn; // 버튼용
extern struct timespec ts;


typedef struct { int r, c; } Point;

// 외부에서 접근 가능한 변수
extern volatile int running;
extern int countA, countB;
int init_gpio();

// 함수 선언
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
