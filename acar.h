#ifndef ACAR_H
#define ACAR_H

#include <MQTTClient.h>
#include <gpiod.h>

// MQTT 설정
#define ADDRESS   "tcp://broker.hivemq.com:1883"
#define CLIENTID  "Vehicle_A"
#define TOPIC_PUB "vehicle/status_A"
#define TOPIC_SUB "vehicle/storage/A"
#define TOPIC_A_DEST        "storage/dest"
#define TOPIC_A_DEST_ARRIVED "storage/dest_arrived"
#define TOPIC_A_COMPLETE     "storage/A_complete"
#define TOPIC_A_COMPLETE_ARRIVED  "storage/A_complete_arrived"

#define QOS     0
#define TIMEOUT 10000L

// 방향 정의
#define NORTH   0
#define EAST    1
#define SOUTH   2
#define WEST    3

// 모터 제어 핀 설정
#define CHIP     "gpiochip0"
#define IN1_PIN  22
#define IN2_PIN  27
#define ENA_PIN  18
#define IN3_PIN  25
#define IN4_PIN  24
#define ENB_PIN  23

// 모터 동작 타이밍
#define SECONDS_PER_GRID_STEP       1.1
#define SECONDS_PER_90_DEG_ROTATION 0.9
#define PRE_ROTATE_FORWARD_CM       8.0f

// 구조체 정의
typedef struct { int r, c; } Point;

typedef struct Node {
    Point pt;
    int g, h, f;
    struct Node *parent;
} Node;

// 전역 변수 선언 (acar.c에서 정의해야 함)
extern MQTTClient client;
extern struct gpiod_chip *chip;
extern struct gpiod_line *in1, *in2, *ena, *in3, *in4, *enb;

extern volatile int has_new_goal;
extern volatile int move_permission;
extern volatile int is_waiting;

extern char current_goal_char;
extern char last_goal_char;

#define ROWS 7
#define COLS 9
#define MAX_PATH 100

extern int grid[ROWS][COLS];
extern Point path[MAX_PATH];
extern int path_len;
extern int path_idx;
extern Point current_pos;
extern int dirA;

// 함수 선언
void motor_go(int speed, double duration);
void motor_stop(void);
void motor_left(int speed, double duration);
void motor_right(int speed, double duration);
extern int check_obstacle(struct gpiod_chip *chip);
void rotate_one(int *dir, int turn_dir, int speed);
void forward_one(Point *pos, int dir, int speed);

Point find_point_by_char(char ch);
int heuristic(Point a, Point b);
int is_valid(int r, int c);
int points_equal(Point a, Point b);
Node *find_lowest_f(Node **open_set, int count);
int in_set(Node **set, int count, Point pt);
void reconstruct_path(Node *curr);
int astar(Point start, Point goal);
void publish_multi_status(Point *path, int idx, int len);
void print_grid_with_dir(Point pos, int dir);
int msgarrvd_a(void *ctx, char *topic, int len, MQTTClient_message *message);
void handle_sigint(int sig);

#endif
