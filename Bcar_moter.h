#ifndef BCAR_SYSTEM_H
#define BCAR_SYSTEM_H

#include <gpiod.h>
#include <MQTTClient.h>
#include "moter_control.h"
#include "encoder.h"

// ===== MQTT 설정 =====
// #define ADDRESS   "ws://mqtt.choidaruhan.xyz:8083"
#define ADDRESS "tcp://broker.hivemq.com:1883"
#define CLIENTID  "Vehicle_B"

#define TOPIC_B               "vehicle/status_B"
#define CMD_B                 "vehicle/storage/B"
#define TOPIC_B_DEST          "storage/b_dest"
#define TOPIC_B_DEST_ARRIVED  "storage/b_dest_arrived"

#define QOS       0
#define TIMEOUT   10000L

// ===== 차량 설정 =====
#define ID        "B"
#define ROWS      7
#define COLS      9
#define MAX_PATH  100
#define MAX_PATH_LENGTH 100

// ===== 구조체/열거형 정의 =====
typedef struct {
    int r;
    int c;
} Point;

typedef struct Node {
    Point pt;
    int g, h, f;
    struct Node* parent;
} Node;

extern Point path[MAX_PATH];
extern int path_len;
extern int path_idx;
extern Point current_pos;
extern int dirB;
extern volatile int move_permission;
extern volatile int is_waiting;
extern volatile int need_replan;

extern MQTTClient client;
extern char current_goal;
extern int new_goal_received;
extern char previous_goal;

// 방향 벡터
extern const int DIR_VECTORS[4][2];

// ===== GPIO 제어/초기화 =====
void delay_ms(int ms);
extern void delay_sec(double sec);
void handle_sigint(int sig);

// ===== 경로탐색 A* 알고리즘 =====
int heuristic(Point a, Point b);
int is_valid(int r, int c);
int points_equal(Point a, Point b);
Node *find_lowest_f(Node **open_set, int count);
int in_set(Node **set, int count, Point pt);
void reconstruct_path(Node *curr);
int astar(Point start, Point goal);

// ===== 유틸리티 =====
void print_grid_with_dir(Point pos, int dir);
void publish_status(Point *path, int idx, int len);
Point find_point_by_char(char ch);
void send_arrival_message(MQTTClient client, char goal);
//int msgarrvd(void *ctx, char *topic, int len, MQTTClient_message *message);

// ===== 파일 기반 경로 처리 =====
int run_vehicle_path(const char *goal);

#endif // BCAR_SYSTEM_H
