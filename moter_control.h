// motor_control.h
#ifndef MOTOR_CONTROL_H
#define MOTOR_CONTROL_H

// 꼭 include 위에 있어야함
typedef enum
{
    N = 0,
    E = 1,
    S = 2,
    W = 3
} Direction;

#include <gpiod.h>
#include <string.h>
#include <MQTTClient.h>
#include "encoder.h"

#define MAX_PATH_LENGTH 100

#define TOPIC_B_COMPLETED "vehicle/B_completed"
#define PERIOD_MS 20          // 20ms 주기 (50Hz)
#define MIN_PULSE_WIDTH 1000  // 0도
#define MID_PULSE_WIDTH 2000  // 90도

extern struct gpiod_line *in1_line;
extern struct gpiod_line *in2_line;
extern struct gpiod_line *ena_line;
extern struct gpiod_line *in3_line;
extern struct gpiod_line *in4_line;
extern struct gpiod_line *enb_line;
extern struct gpiod_line *line_btn;
extern struct gpiod_line *servo_line;

extern struct gpiod_line *trig_line;
extern struct gpiod_line *echo_line;
typedef struct
{
    int x;
    int y;
} Position;

// 외부에서 사용할 함수 선언


void delay_ms(int ms);
void pwm_set_duty(struct gpiod_line *line, int duty_percent);
void generate_pwm(struct gpiod_line *line, int pulse_width_us, int duration_ms) ;
int angle_to_pulse(int angle);
void move_servo(struct gpiod_line *line, int angle);
void setup();
void cleanup();
void set_speed(int speedA, int speedB);
float get_distance_cm();
void generate_pwm(struct gpiod_line *line, int pulse_width_us, int duration_ms);

Direction move_step(Position curr, Position next, Direction current_dir);
int load_path_from_file(const char *filename, Position path[]);
int complete_message(const char *topic, const char *message);
int run_vehicle_path(const char *goal);

// 방향 벡터 배열 (N/E/S/W)
extern const int DIR_VECTORS[4][2];

#endif
