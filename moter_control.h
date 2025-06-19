// motor_control.h
#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

#include <gpiod.h>
#include <string.h>
#include <MQTTClient.h>

#define MAX_PATH_LENGTH 100
// BCM GPIO 번호 정의 (libgpiod는 gpiochip0 기준 라인번호 = BCM 번호)
#define IN1_PIN 17
#define IN2_PIN 18
#define ENA_PIN 12
#define IN3_PIN 22
#define IN4_PIN 23
#define ENB_PIN 13

// 초음파 센서 핀 (예시, 필요시 변경)
#define TRIG_PIN 6
#define ECHO_PIN 5

#define TOPIC_B_COMPLETED "vehicle/B_completed"

extern struct gpiod_chip *chip;
extern struct gpiod_line *in1_line;
extern struct gpiod_line *in2_line;
extern struct gpiod_line *ena_line;
extern struct gpiod_line *in3_line;
extern struct gpiod_line *in4_line;
extern struct gpiod_line *enb_line;

extern struct gpiod_line *trig_line;
extern struct gpiod_line *echo_line;
typedef struct
{
    int x;
    int y;
} Position;

typedef enum
{
    N =0, 
    E = 1,
    S = 2,
    W = 3
} Direction;


// 외부에서 사용할 함수 선언
void setup();
void cleanup();
void delay_ms(int ms);
void pwm_set_duty(struct gpiod_line *line, int duty_percent);
void set_speed(int speedA, int speedB);
float get_distance_cm();
unsigned long get_microseconds();

Direction move_step(Position curr, Position next, Direction current_dir);
int load_path_from_file(const char *filename, Position path[]);
int run_vehicle_path(const char *goal);

// 방향 벡터 배열 (N/E/S/W)
extern const int DIR_VECTORS[4][2];

#endif
