/*
컴파일 :
gcc acar.c -o acar -lpaho-mqtt3c -lwiringPi
실행   :
./acar
*/

#include <stdio.h>                // 표준 입출력 헤더
#include <stdlib.h>               // 메모리 할당, 변환 함수 등
#include <string.h>               // 문자열 처리 함수
#include <unistd.h>               // usleep 함수
#include <signal.h>               // SIGINT 처리
#include "MQTTClient.h"           // Paho MQTT C 클라이언트
#include <sys/types.h>            // select, fd_set 사용
#include <sys/select.h>           // select 함수 사용
#include <wiringPi.h>             // GPIO 제어
#include <softPwm.h>              // 소프트 PWM

// 방향 정의
#define NORTH   0
#define EAST    1
#define SOUTH   2
#define WEST    3

// MQTT 설정
#define ADDRESS   "tcp://localhost:1883"
#define CLIENTID  "Vehicle_A"
#define TOPIC_PUB "vehicle/status_A"
#define TOPIC_SUB "vehicle/storage/A"
#define QOS       0
#define TIMEOUT   10000L

// 그리드 설정
#define ROWS      7
#define COLS      9
#define MAX_PATH 100

// 모터 제어 핀 설정 (WiringPi BCM 모드)
#define AIN1 22
#define AIN2 27
#define PWMA 18
#define BIN1 25
#define BIN2 24
#define PWMB 23

// 모터 동작 타이밍 (초)
#define SECONDS_PER_GRID_STEP       1.1
#define SECONDS_PER_90_DEG_ROTATION 0.8
#define PRE_ROTATE_FORWARD_CM       8.0f

// 점 좌표 구조체
typedef struct { int r, c; } Point;

// A* 노드 구조체
typedef struct Node {
    Point pt;           // 현재 위치
    int g, h, f;        // g: 시작->현재, h: 휴리스틱, f=g+h
    struct Node *parent; // 부모 노드 포인터
} Node;

// 전역 상태
static MQTTClient client;
static int grid[ROWS][COLS] = {
    {'A',0,0,0,0,0,0,0,0},
    {0,1,1,1,0,1,1,1,0},
    {0,1,'S',1,0,1,'G',1,0},
    {0,0,0,0,0,0,0,0,0},
    {0,1,1,1,0,1,1,1,0},
    {0,1,'K',1,0,1,'W',1,0},
    {0,0,0,0,0,0,0,0,'B'}
};
static Point path[MAX_PATH];            // 계산된 경로 저장
static int path_len = 0;                // 경로 길이
static int path_idx = 0;                // 경로 인덱스
static Point current_pos = {0, 0};      // B 차량 초기 위치
static int dirA = SOUTH;                // B 차량 초기 방향
static volatile int move_permission = 0;
static volatile int is_waiting = 0;
static char goalsA[] = {'K','G','S','W','A'};

// 함수 프로토타입
static void handle_sigint(int sig);
static void delay_sec(double sec);
static void motor_go(void);
static void motor_stop(void);
static void motor_right(void);
static void motor_left(void);
static void rotate_one(int *dir, int turn_dir);
static void forward_one(Point *pos, int dir);
Point  find_point_by_char(char ch);
int    heuristic(Point a, Point b);
int    is_valid(int r, int c);
int    points_equal(Point a, Point b);
Node  *find_lowest_f(Node **open_set, int count);
int    in_set(Node **set, int count, Point pt);
void   reconstruct_path(Node *curr);
int    astar(Point start, Point goal);
void   publish_multi_status(Point *path, int idx, int len);
void   print_grid_with_dir(Point pos, int dir);
int    msgarrvd(void *ctx, char *topic, int len, MQTTClient_message *message);

// SIGINT 핸들러
static void handle_sigint(int sig) {
    (void)sig;
    motor_stop();
    exit(0);
}

// 지연 함수
static void delay_sec(double sec) {
    usleep((unsigned)(sec * 1e6));
}

// 모터 제어 함수
static void motor_go(void) {
    printf("[motor] GO\n");   fflush(stdout);
    digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
    softPwmWrite(PWMA, 100);  softPwmWrite(PWMB, 100);
}

static void motor_stop(void) {
    printf("[motor] STOP\n"); fflush(stdout);
    digitalWrite(AIN1, LOW);  digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);  digitalWrite(BIN2, LOW);
    softPwmWrite(PWMA, 0);    softPwmWrite(PWMB, 0);
}

static void motor_right(void) {
    printf("[motor] RIGHT\n"); fflush(stdout);
    digitalWrite(AIN1, LOW);  digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, HIGH); digitalWrite(BIN2, LOW);
    softPwmWrite(PWMA, 100);  softPwmWrite(PWMB, 100);
}

static void motor_left(void) {
    printf("[motor] LEFT\n"); fflush(stdout);
    digitalWrite(AIN1, HIGH); digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);  digitalWrite(BIN2, HIGH);
    softPwmWrite(PWMA, 100);  softPwmWrite(PWMB, 100);
}

static void rotate_one(int *dir, int turn_dir) {
    double t0 = (PRE_ROTATE_FORWARD_CM / 30.0f) * SECONDS_PER_GRID_STEP;
    motor_go();
    delay_sec(t0);
    motor_stop();
    delay_sec(0.1);
    if (turn_dir > 0)
        motor_right();
    else
        motor_left();
    delay_sec(SECONDS_PER_90_DEG_ROTATION);
    motor_stop();
    *dir = (*dir + turn_dir + 4) % 4;
}

static void forward_one(Point *pos, int dir) {
    motor_go();
    delay_sec(SECONDS_PER_GRID_STEP);
    motor_stop();
    switch (dir) {
        case NORTH: pos->r--; break;
        case SOUTH: pos->r++; break;
        case EAST:  pos->c++; break;
        case WEST:  pos->c--; break;
    }
}

// 휴리스틱: 맨해튼 거리
int heuristic(Point a, Point b) {
    return abs(a.r - b.r) + abs(a.c - b.c);
}

// 셀 유효성 검사
int is_valid(int r, int c) {
    return (r >= 0 && r < ROWS && c >= 0 && c < COLS && grid[r][c] != 1);
}

// 좌표 비교
int points_equal(Point a, Point b) {
    return (a.r == b.r && a.c == b.c);
}

// 최소 f 값 노드 선택
Node *find_lowest_f(Node **open_set, int count) {
    Node *best = open_set[0];
    for (int i = 1; i < count; i++)
        if (open_set[i]->f < best->f)
            best = open_set[i];
    return best;
}

// 집합 내 좌표 존재 확인
int in_set(Node **set, int count, Point pt) {
    for (int i = 0; i < count; i++)
        if (points_equal(set[i]->pt, pt))
            return i;
    return -1;
}

// 경로 복원
void reconstruct_path(Node *curr) {
    path_len = 0;
    while (curr && path_len < MAX_PATH) {
        path[path_len++] = curr->pt;
        curr = curr->parent;
    }
    for (int i = 0; i < path_len/2; i++) {
        Point tmp = path[i];
        path[i] = path[path_len-1-i];
        path[path_len-1-i] = tmp;
    }
}

int astar(Point start, Point goal) {
    Node *open_set[ROWS*COLS]; int oc=0;
    Node *closed_set[ROWS*COLS]; int cc=0;
    Node *sn = malloc(sizeof(Node));
    sn->pt = start;
    sn->g = 0;
    sn->h = heuristic(start, goal);
    sn->f = sn->g + sn->h;
    sn->parent = NULL;
    open_set[oc++] = sn;

    while (oc > 0) {
        Node *curr = find_lowest_f(open_set, oc);
        if (points_equal(curr->pt, goal)) {
            reconstruct_path(curr);
            break;
        }
        int idx = in_set(open_set, oc, curr->pt);
        open_set[idx] = open_set[--oc];
        closed_set[cc++] = curr;

        int dr[4] = {-1,1,0,0}, dc[4] = {0,0,-1,1};
        for (int i = 0; i < 4; i++) {
            Point np = { curr->pt.r + dr[i], curr->pt.c + dc[i] };
            if (!is_valid(np.r,np.c) || in_set(closed_set,cc,np) != -1)
                continue;

            int ng = curr->g + 1;
            int fi = in_set(open_set, oc, np);
            if (fi < 0) {
                Node *nn = malloc(sizeof(Node));
                nn->pt = np;
                nn->g = ng;
                nn->h = heuristic(np, goal);
                nn->f = nn->g + nn->h;
                nn->parent = curr;
                open_set[oc++] = nn;
            } else if (ng < open_set[fi]->g) {
                open_set[fi]->g = ng;
                open_set[fi]->f = ng + open_set[fi]->h;
                open_set[fi]->parent = curr;
            }
        }
    }

    for (int i = 0; i < oc; i++) free(open_set[i]);
    for (int i = 0; i < cc; i++) free(closed_set[i]);
    return (path_len > 0);
}

void publish_multi_status(Point *path, int idx, int len) {
    char payload[256], positions[128] = "";
    int count=0;
    for (int i = idx-1; i <= len && count<4; i++,count++) {
        if (i < 0)
            snprintf(positions+strlen(positions), sizeof(positions)-strlen(positions),
                     "(%d,%d),", current_pos.r, current_pos.c);
        else
            snprintf(positions+strlen(positions), sizeof(positions)-strlen(positions),
                     "(%d,%d),", path[i].r, path[i].c);
    }
    if (positions[strlen(positions)-1] == ',')
        positions[strlen(positions)-1] = '\0';

    snprintf(payload, sizeof(payload), "POS: (%d,%d) PATH: [%s]",
             current_pos.r, current_pos.c, positions);
    printf("[송신] A -> %s\n", payload);

    MQTTClient_message msg = MQTTClient_message_initializer;
    msg.payload    = payload;
    msg.payloadlen = strlen(payload);
    msg.qos        = QOS;
    msg.retained   = 0;
    MQTTClient_publishMessage(client, TOPIC_PUB, &msg, NULL);
}

void print_grid_with_dir(Point pos, int dir) {
    char arrow[4] = {'^','>','v','<'};
    printf("   "); for (int c=0; c<COLS; c++) printf("%d ", c); puts("");
    for (int r=0; r<ROWS; r++) {
        printf("%d: ", r);
        for (int c=0; c<COLS; c++) {
            if (r==pos.r && c==pos.c)
                printf("%c ", arrow[dir]);
            else if (grid[r][c]==1)
                printf("# ");
            else
                printf(". ");
        }
        puts("");
    }
    puts("");
}

Point find_point_by_char(char ch) {
    for (int r=0; r<ROWS; r++)
        for (int c=0; c<COLS; c++)
            if (grid[r][c]==ch)
                return (Point){r,c};
    return (Point){-1,-1};
}

int msgarrvd(void *ctx, char *topic, int len, MQTTClient_message *message) {
    char buf[message->payloadlen+1];
    memcpy(buf, message->payload, message->payloadlen);
    buf[message->payloadlen]='\0';
    printf("[수신] %s\n", buf);
    if (!strcmp(buf,"move")) { 
        is_waiting=0; 
        move_permission=1; 
        puts(">> move");
     } else if (!strcmp(buf,"hold")) { 
        is_waiting=1; 
        move_permission=0; 
        puts(">> hold"); 
    }
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topic);
    return 1;
}

int main(void) {
    signal(SIGINT, handle_sigint);
    wiringPiSetupGpio();
    pinMode(AIN1, OUTPUT); pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT); pinMode(BIN2, OUTPUT);
    softPwmCreate(PWMA,0,100); softPwmCreate(PWMB,0,100);

    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, NULL, msgarrvd, NULL);
    if (MQTTClient_connect(client, &opts) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "MQTT 연결 실패\n");
        return 1;
    }
    MQTTClient_subscribe(client, TOPIC_SUB, QOS);

    int n = sizeof(goalsA)/sizeof(goalsA[0]);
    for (int i=0; i<n; i++) {
        Point g = find_point_by_char(goalsA[i]);
        if (!astar(current_pos, g)) {
            printf("경로 탐색 실패: %c\n", goalsA[i]);
            continue;
        }
        path_idx = 0;
        publish_multi_status(path, path_idx, path_len);

        while (path_idx < path_len) {
            while (is_waiting || !move_permission) {
                MQTTClient_yield();
                usleep(200000);
            }
            move_permission = 0;

            Point nxt = path[path_idx];
            int td = (nxt.r < current_pos.r ? NORTH :
                      nxt.r > current_pos.r ? SOUTH :
                      nxt.c > current_pos.c ? EAST  : WEST);
            int diff = (td - dirA + 4) % 4;
            if (diff == 3) diff = -1;

            if (diff<0) {
                puts("[A] TURN_LEFT");
                rotate_one(&dirA, -1);
            } else if (diff>0) {
                puts("[A] TURN_RIGHT");
                rotate_one(&dirA, +1);
            } else {
                puts("[A] FORWARD");
                motor_go(); 
                delay_sec(SECONDS_PER_GRID_STEP); 
                motor_stop();
                current_pos = nxt;
                path_idx++;
            }

            publish_multi_status(path, path_idx, path_len);
            print_grid_with_dir(current_pos, dirA);
        }
    }

    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);
    return 0;
}
