/*
컴파일 :
gcc acar.c -o acar -lpaho-mqtt3c -lgpiod
실행   :
./acar
*/

#include <stdio.h>                // 표준 입출력 헤더
#include <stdlib.h>               // 메모리 할당, 변환 함수 등
#include <string.h>               // 문자열 처리 함수
#include <unistd.h>               // usleep 함수
#include <signal.h>               // SIGINT 처리
#include <MQTTClient.h>           // Paho MQTT C 클라이언트
#include <gpiod.h>
#include <sys/types.h>            // select, fd_set 사용
#include <sys/select.h>           // select 함수 사용

// MQTT 설정
#define ADDRESS   "tcp://broker.hivemq.com:1883"
#define CLIENTID  "Vehicle_A"
#define TOPIC_PUB "vehicle/status_A"
#define TOPIC_SUB "vehicle/storage/A"

#define TOPIC_A_DEST        "storage/dest"   // 목적지 출발 알림용 토픽
#define TOPIC_A_DEST_ARRIVED     "storage/dest_arrived"     // 목적지 도착 알림용 토픽
#define QOS       0
#define TIMEOUT   10000L

// 방향 정의
#define NORTH   0
#define EAST    1
#define SOUTH   2
#define WEST    3


// 그리드 설정
#define ROWS      7
#define COLS      9
#define MAX_PATH 100

// 모터 제어 핀 설정
#define CHIP "gpiochip0"
#define IN1_PIN 22
#define IN2_PIN 27
#define ENA_PIN 18
#define IN3_PIN 25
#define IN4_PIN 24
#define ENB_PIN 23

// 모터 동작 타이밍 (초)
#define SECONDS_PER_GRID_STEP       1.1
#define SECONDS_PER_90_DEG_ROTATION 0.9
#define PRE_ROTATE_FORWARD_CM       8.0f

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

// 점 좌표 구조체
typedef struct { int r, c; } Point;

// A* 노드 구조체
typedef struct Node {
    Point pt;           // 현재 위치
    int g, h, f;        // g: 시작->현재, h: 휴리스틱, f=g+h
    struct Node *parent; // 부모 노드 포인터
} Node;

static struct gpiod_chip *chip;
static struct gpiod_line *in1, *in2, *ena, *in3, *in4, *enb;
static MQTTClient client;

// 프로토타입
static void handle_sigint(int sig);
static void motor_control(int in1_val, int in2_val, int in3_val, int in4_val, int pwm_a, int pwm_b, double duration_sec);
static void motor_go(int speed, double duration);
static void motor_stop(void);
static void motor_right(int speed, double duration);
static void motor_left(int speed, double duration);
static void rotate_one(int *dir, int turn_dir, int speed);
static void forward_one(Point *pos, int dir, int speed);

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

// 전역 변수 추가
static volatile int has_new_goal = 0;
static char current_goal_char = '\0';
static char last_goal_char = '\0';

static Point path[MAX_PATH];            // 계산된 경로 저장
static int path_len = 0;                // 경로 길이
static int path_idx = 0;                // 경로 인덱스
static Point current_pos = {0, 0};      // A 차량 초기 위치
static int dirA = SOUTH;                // A 차량 초기 방향
static volatile int move_permission = 0;
static volatile int is_waiting = 0;

// SIGINT 핸들러
static void handle_sigint(int sig) {
    gpiod_line_set_value(ena, 0);
    gpiod_line_set_value(enb, 0);
    gpiod_chip_close(chip);
    exit(0);
}

static void delay_sec(double sec) {
    usleep((unsigned)(sec * 1e6));
}

void motor_stop() {
    gpiod_line_set_value(ena, 0);
    gpiod_line_set_value(enb, 0);
}

void motor_go(int speed, double duration) {
    gpiod_line_set_value(in1, 0);
    gpiod_line_set_value(in2, 1);
    gpiod_line_set_value(in3, 0);
    gpiod_line_set_value(in4, 1);
    gpiod_line_set_value(ena, 1);
    gpiod_line_set_value(enb, 1);
    usleep(duration * 1e6);
    motor_stop();
}

void motor_left(int speed, double duration) {
    gpiod_line_set_value(in1, 1);
    gpiod_line_set_value(in2, 0);
    gpiod_line_set_value(in3, 0);
    gpiod_line_set_value(in4, 1);
    gpiod_line_set_value(ena, 1);
    gpiod_line_set_value(enb, 1);
    usleep(duration * 1e6);
    motor_stop();
}

void motor_right(int speed, double duration) {
    gpiod_line_set_value(in1, 0);
    gpiod_line_set_value(in2, 1);
    gpiod_line_set_value(in3, 1);
    gpiod_line_set_value(in4, 0);
    gpiod_line_set_value(ena, 1);
    gpiod_line_set_value(enb, 1);
    usleep(duration * 1e6);
    motor_stop();
}

void rotate_one(int *dir, int turn_dir, int speed) {
    double t0 = (PRE_ROTATE_FORWARD_CM / 30.0f) * SECONDS_PER_GRID_STEP;
    motor_go(speed, t0);
    usleep(100000);
    if (turn_dir > 0)
        motor_right(speed, SECONDS_PER_90_DEG_ROTATION);
    else
        motor_left(speed, SECONDS_PER_90_DEG_ROTATION);
    *dir = (*dir + turn_dir + 4) % 4;
}

void forward_one(Point *pos, int dir, int speed) {
    motor_go(speed, SECONDS_PER_GRID_STEP);
    switch (dir) {
        case NORTH: pos->r--; break;
        case EAST:  pos->c++; break;
        case SOUTH: pos->r++; break;
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
    // 부모 링크를 따라 역방향으로 경로를 tmp에 저장
    Point tmp[MAX_PATH];
    int len = 0;
    while (curr && len < MAX_PATH) {
        tmp[len++] = curr->pt;
        curr = curr->parent;
    }
    // 경로가 없으면 종료
    if (len == 0) {
        path_len = 0;
        return;
    }
    // 시작 위치(tmp[len-1])를 제외한 실제 이동 경로를 순서대로 복원
    path_len = len - 1;
    for (int i = 0; i < path_len; i++) {
        path[i] = tmp[len - 2 - i];
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

int publish_message(const char* topic, const char* payload) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void*)payload;
    pubmsg.payloadlen = strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("[오류] 메시지 발행 실패: %d\n", rc);
        return rc;
    }

    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    return rc;
}

int msgarrvd(void *ctx, char *topic, int len, MQTTClient_message *message) {
    char buf[message->payloadlen+1];
    memcpy(buf, message->payload, message->payloadlen);
    buf[message->payloadlen]='\0';
    
    if(strcmp(topic,TOPIC_SUB)==0)
    {
        printf("[수신] %s\n", buf);
        if (!strcmp(buf,"move")) { 
            is_waiting=0;
            has_new_goal=1;
            move_permission=1; 
            puts(">> move");
        } else if (!strcmp(buf,"hold")) { 
            is_waiting=1; 
            move_permission=0; 
            puts(">> hold"); 
        }
    }
    if(strcmp(topic,TOPIC_A_DEST)==0)
    {
        char dest_char = buf[0];  // 문자열의 첫 문자만 추출
        if (dest_char != '\0') 
        {
            if (dest_char == last_goal_char) {
                printf(">> 동일한 목적지 구역입니다: %c\n", dest_char);
            } else {
                current_goal_char = dest_char;
                last_goal_char = dest_char;
                has_new_goal=1;
                printf(">> 새 목적지 수신: %c\n", current_goal_char);
            }
        } 
        else 
        {
            printf(">> 알 수 없는 목적지 코드: %s\n", buf);
        }
    }
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topic);
    return 1;
}

int main(void) {
    signal(SIGINT, handle_sigint);

    chip = gpiod_chip_open_by_name(CHIP);
    // ENA/ENB 출력 설정
    ena = gpiod_chip_get_line(chip, ENA_PIN);
    enb = gpiod_chip_get_line(chip, ENB_PIN);
    gpiod_line_request_output(ena, "ENA", 1);
    gpiod_line_request_output(enb, "ENB", 1);

    // 방향 제어 핀
    in1 = gpiod_chip_get_line(chip, IN1_PIN);
    in2 = gpiod_chip_get_line(chip, IN2_PIN);
    in3 = gpiod_chip_get_line(chip, IN3_PIN);
    in4 = gpiod_chip_get_line(chip, IN4_PIN);

    in1 = gpiod_chip_get_line(chip, IN1_PIN);
    in2 = gpiod_chip_get_line(chip, IN2_PIN);
    in3 = gpiod_chip_get_line(chip, IN3_PIN);
    in4 = gpiod_chip_get_line(chip, IN4_PIN);
    
    // 방향제어 핀들을 출력으로 설정
    if (gpiod_line_request_output(in1, "IN1", 0) < 0 ||
    gpiod_line_request_output(in2, "IN2", 0) < 0 ||
    gpiod_line_request_output(in3, "IN3", 0) < 0 ||
    gpiod_line_request_output(in4, "IN4", 0) < 0) {
    perror("IN 핀 설정 실패");
    return 1;
    }

    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, NULL, msgarrvd, NULL);
    
    if (MQTTClient_connect(client, &opts) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "MQTT 연결 실패\n");
        return 1;
    }
    MQTTClient_subscribe(client, TOPIC_SUB, QOS);
    MQTTClient_subscribe(client, TOPIC_A_DEST, QOS);

    while (1) {
        MQTTClient_yield();  // 항상 메시지 처리 유지
        usleep(100000);      // CPU 사용률 감소용 대기

        if (!has_new_goal) continue;

        Point g = find_point_by_char(current_goal_char);
        if (!astar(current_pos, g)) {
            printf("경로 탐색 실패: %c\n", current_goal_char);
            has_new_goal = 0;
            continue;
        }

        path_idx = 0;
        publish_multi_status(path, path_idx, path_len);
        has_new_goal = 0; // 목표 수신 완료 후 초기화

        while (path_idx < path_len) 
        {
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

            if (diff < 0) {
                puts("[A] TURN_LEFT");
                rotate_one(&dirA, -1, 60);  // 속도 70으로 좌회전
            } else if (diff > 0) {
                puts("[A] TURN_RIGHT");
                rotate_one(&dirA, +1, 60);  // 속도 70으로 우회전
            } else {
                puts("[A] FORWARD");
                forward_one(&current_pos, dirA, 60);  // 속도 70으로 전진
                path_idx++;
            }

            publish_multi_status(path, path_idx, path_len);
            print_grid_with_dir(current_pos, dirA);
        }

        //printf(">> 목적지 %c 도착 완료. 다음 목적지 대기 중...\n", current_goal_char);
        // 도착 메시지 송신
        char msg_buffer[100];
        sprintf(msg_buffer, "%c", current_goal_char);  // 도착한 목적지 문자를 메시지에 저장
        if (publish_message(TOPIC_A_DEST_ARRIVED, msg_buffer) == MQTTCLIENT_SUCCESS) {
            printf("[송신] %s → %s\n", msg_buffer, TOPIC_A_DEST_ARRIVED);
        } else {
            printf("[오류] 목적지 도착 메시지 전송 실패: %s\n", msg_buffer);
        }

        current_goal_char = '\0';
    }

    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);
    return 0;
}
