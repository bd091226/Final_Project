/*
컴파일 :
gcc bcar.c -o bcar -lpaho-mqtt3c -lgpiod
실행   :
./bcar
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <MQTTClient.h>
#include <gpiod.h>
#include <sys/types.h>
#include <sys/select.h>
#include "moter_control.h"
#include "Bcar_moter.h"



Point path[MAX_PATH];            // 계산된 경로 저장
int   path_len = 0;              // 경로 길이
int   path_idx = 0;              // 경로 인덱스
Point current_pos = {6, 8};      // B 차량 초기 위치
Direction dirB = N; // B 차량 초기 방향
volatile int move_permission = 0;
volatile int is_waiting = 0;
volatile int need_replan = 0;

// 전역 변수
MQTTClient client;
char current_goal = '\0';
int new_goal_received = 0;
char previous_goal = '\0';

int grid[ROWS][COLS] = {
    {'A',0,0,0,0,0,0,0,0},
    {0,1,1,1,0,1,1,1,0},
    {0,1,'S',1,0,1,'G',1,0},
    {0,0,0,0,0,0,0,0,0},
    {0,1,1,1,0,1,1,1,0},
    {0,1,'K',1,0,1,'W',1,0},
    {0,0,0,0,0,0,0,0,'B'}
};

// void handle_sigint(int sig) {
//     cleanup();
//     gpiod_line_set_value(ena_line, 0);
//     gpiod_line_set_value(enb_line, 0);
//     gpiod_chip_close(chip);
//     exit(0);
// }
// void handle_sigint(int sig) {
//     printf("\n🛑 SIGINT 감지, 프로그램 종료 중...\n");
//     cleanup();  // 리소스 해제 함수
//     exit(0);
// }

void delay_sec(double sec) {
    usleep((unsigned)(sec * 1e6));
}

void motor_control(int in1_val, int in2_val, int in3_val, int in4_val, int pwm_a, int pwm_b, double duration_sec) {
    int cycle_us = 2000;
    int cycles = (duration_sec * 1e6) / cycle_us;

    int on_time_a = (cycle_us * pwm_a) / 100;
    int off_time_a = cycle_us - on_time_a;
    int on_time_b = (cycle_us * pwm_b) / 100;
    int off_time_b = cycle_us - on_time_b;

    gpiod_line_set_value(in1_line, in1_val);
    gpiod_line_set_value(in2_line, in2_val);
    gpiod_line_set_value(in3_line, in3_val);
    gpiod_line_set_value(in4_line, in4_val);

    for (int i = 0; i < cycles; i++) {
        if (pwm_a > 0) gpiod_line_set_value(ena_line, 1);
        if (pwm_b > 0) gpiod_line_set_value(enb_line, 1);

        usleep((on_time_a < on_time_b) ? on_time_a : on_time_b);

        if (pwm_a < 100) gpiod_line_set_value(ena_line, 0);
        if (pwm_b < 100) gpiod_line_set_value(enb_line, 0);

        usleep((off_time_a > off_time_b) ? off_time_a : off_time_b);
    }

    gpiod_line_set_value(ena_line, 0);
    gpiod_line_set_value(enb_line, 0);
}

void motor_go(int speed, double duration) {
    motor_control(1, 0, 1, 0, speed, speed, duration);
}

void motor_stop(void) {
    motor_control(0, 0, 0, 0, 0, 0, 0.1);
}

static void motor_left(int speed, double duration) {
    motor_control(0, 1, 1, 0, speed, speed, duration);
}

static void motor_right(int speed, double duration) {
    motor_control(1, 0, 0, 1, speed, speed, duration);
}

void rotate_one(Direction *dir, int turn_dir, int speed) {
    double t0 = (PRE_ROTATE_FORWARD_CM / 30.0f) * 1.1;
    motor_go(speed, t0);                 // 회전 전 전진 보정
    motor_stop();
    delay_sec(0.1);
    if (turn_dir > 0)
        motor_right(speed, SECONDS_PER_90_DEG_ROTATION); // 우회전
    else
        motor_left(speed, SECONDS_PER_90_DEG_ROTATION);  // 좌회전
    motor_stop();
    *dir = (*dir + turn_dir + 4) % 4;  // 방향 갱신
}

void forward_one(Point *pos, int dir, int speed) {
    motor_go(speed, SECONDS_PER_GRID_STEP);                 // 전진
    motor_stop();
    switch (dir) {
        case 0: pos->r--; break;
        case 1: pos->c++; break;
        case 2: pos->r++; break;
        case 3: pos->c--; break;
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
    for (int i = 1; i < count; i++) {
        if (open_set[i]->f < best->f)
            best = open_set[i];
    }
    return best;
}

// 집합 내 좌표 존재 확인
int in_set(Node **set, int count, Point pt) {
    for (int i = 0; i < count; i++) {
        if (points_equal(set[i]->pt, pt))
            return i;
    }
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

// A* 경로 탐색
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

// 상태 발행
void publish_status(Point *path, int idx, int len) {
    char payload[256];
    char pts[128] = "";
    int cnt = 0;
    for (int i = idx; i < idx+4 && i < len; i++, cnt++) {
        snprintf(pts + strlen(pts), sizeof(pts) - strlen(pts),
                 "(%d,%d)%s", path[i].r, path[i].c, (i<idx+3)?",":"");
    }
    // 포맷: POS: (r,c) PATH: [..]
    sprintf(payload, "ID : %S POS: (%d,%d) PATH: [%s]", ID, current_pos.r, current_pos.c, pts);
    printf("[송신] B -> %s\n", payload);

    MQTTClient_message msg = MQTTClient_message_initializer;
    msg.payload    = payload;
    msg.payloadlen = strlen(payload);
    msg.qos        = QOS;
    msg.retained   = 0;
    MQTTClient_publishMessage(client, TOPIC_B, &msg, NULL);
}

// 그리드 + 방향 출력
void print_grid_with_dir(Point pos, int dir) {
    char arr[4] = {'^','>','v','<'};
    printf("   "); 
    for (int c = 0; c < COLS; c++) 
        printf("%d ", c); 
    puts("");
    for (int r = 0; r < ROWS; r++) {
        printf("%d: ", r);
        for (int c = 0; c < COLS; c++) {
            if (r == pos.r && c == pos.c)
                printf("%c ", arr[dir]);
            else if (grid[r][c] == 1)
                printf("# ");
            else
                printf(". ");
        }
        puts("");
    }
    puts("");
}

// 좌표 찾기
Point find_point_by_char(char ch) {
    for (int r = 0; r < ROWS; r++) {
        for (int c = 0; c < COLS; c++) {
            if (grid[r][c] == ch)
                return (Point){r, c};
        }
    }
    return (Point){-1, -1};
}
void send_arrival_message(MQTTClient client, char goal) 
{
    if(current_pos.r == 6 && current_pos.c == 8) 
    {
        char goal_str[2] = {goal, '\0'}; // 문자열로 변환
        printf("%s 집하센터로 출발\n",goal_str);
        run_vehicle_path(goal_str);
    }
    else{
        char msg[2] = {goal, '\0'};
        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = msg;
        pubmsg.payloadlen = strlen(msg);
        pubmsg.qos = QOS;
        pubmsg.retained = 0;

        MQTTClient_deliveryToken token;
        MQTTClient_publishMessage(client, TOPIC_B_DEST_ARRIVED, &pubmsg, &token);
        //MQTTClient_waitForCompletion(client, token, TIMEOUT);
        printf("[송신] : %s -> %s \n", msg, TOPIC_B_DEST_ARRIVED);
    }
    
}

// 콜백 처리
// int msgarrvd(void *ctx, char *topic, int len, MQTTClient_message *message) {
//     char buf[message->payloadlen+1];
//     memcpy(buf, message->payload, message->payloadlen);
//     buf[message->payloadlen] = '\0';
//     printf("[수신] %s -> %s \n", topic,buf);
//     if (!strcmp(buf, "move")) 
//     {
//         is_waiting = 0; 
//         move_permission = 1; 
//         puts(">> move");
//     // } else if (!strcmp(buf, "wait")) {
//     //     is_waiting = 1; 
//     //     puts(">> wait");
//     // } else if (!strcmp(buf, "replan")) {
//     //     need_replan = 1; 
//     //     puts(">> replan");
//     }
//     if(strcmp(topic, TOPIC_B_DEST) == 0) 
//     {
//         current_goal = buf[0];           // 수신한 목적지 저장 (ex. 'K')
//         new_goal_received = 1;           // 목적지 수신 플래그 설정
//     }
//     MQTTClient_freeMessage(&message);
//     MQTTClient_free(topic);
//     return 1;
// }

// int main(void) {
//     signal(SIGINT, handle_sigint);

//     chip = gpiod_chip_open_by_name(CHIP);
//     in1_line = gpiod_chip_get_line(chip, IN1_PIN);
//     in2_line = gpiod_chip_get_line(chip, IN2_PIN);
//     ena_line = gpiod_chip_get_line(chip, ENA_PIN);
//     in3_line = gpiod_chip_get_line(chip, IN3_PIN);
//     in4_line = gpiod_chip_get_line(chip, IN4_PIN);
//     enb_line = gpiod_chip_get_line(chip, ENB_PIN);

//     gpiod_line_request_output(in1_line, "IN1", 0);
//     gpiod_line_request_output(in2_line, "IN2", 0);
//     gpiod_line_request_output(ena_line, "ENA", 0);
//     gpiod_line_request_output(in3_line, "IN3", 0);
//     gpiod_line_request_output(in4_line, "IN4", 0);
//     gpiod_line_request_output(enb_line, "ENB", 0);
    
//     MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
//     MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
//     MQTTClient_setCallbacks(client, NULL, NULL, msgarrvd, NULL);

//     if (MQTTClient_connect(client, &opts) != MQTTCLIENT_SUCCESS) {
//         fprintf(stderr, "MQTT 연결 실패\n");
//         return 1;
//     }
//     MQTTClient_subscribe(client, CMD_B, QOS);
//     MQTTClient_subscribe(client, TOPIC_B_DEST, QOS);

//     while (1)
//     {
//         MQTTClient_yield(); // 콜백을 실행시키기 위한 함수

//         if (new_goal_received && current_goal != '\0')
//         {
//             printf("➡️  A* 경로 탐색 시작: 목적지 '%c'\n", current_goal);

//             Point goal = find_point_by_char(current_goal);
//             if (!astar(current_pos, goal))
//             {
//                 printf("❌ 경로 탐색 실패: %c\n", current_goal);
//                 new_goal_received = 0;
//                 continue;
//             }

//             path_idx = 0;
//             publish_status(path, path_idx, path_len);

//             while (path_idx < path_len)
//             {
//                 while (is_waiting || !move_permission)
//                 {
//                     MQTTClient_yield();
//                     usleep(200000);
//                 }
//                 move_permission = 0;

//                 Point nxt = path[path_idx];
//                 int td = (nxt.r < current_pos.r ? N :
//                           nxt.r > current_pos.r ? S :
//                           nxt.c > current_pos.c ? E  : W);
//                 int diff = (td - dirB + 4) % 4;
//                 if (diff == 3) diff = -1;

//                 if (diff < 0)
//                 {
//                     puts("[B] TURN_LEFT");
//                     rotate_one(&dirB, -1, 60);
//                 }
//                 else if (diff > 0)
//                 {
//                     puts("[B] TURN_RIGHT");
//                     rotate_one(&dirB, +1, 60);
//                 }
//                 else
//                 {
//                     puts("[B] FORWARD");
//                     forward_one(&current_pos, dirB, 60);
//                     path_idx++;
//                 }

//                 publish_status(path, path_idx, path_len);
//                 print_grid_with_dir(current_pos, dirB);
//             }
//             if(current_goal=='B')
//             {
//                 //cleanup();
//                 send_arrival_message(client, previous_goal);
                
//             }
//             else{
//                 send_arrival_message(client, current_goal);
                
//             }
            
//             previous_goal = current_goal;
//             new_goal_received = 0; // 현재 목적지 처리가 끝났으므로 플래그 리셋
//             current_goal = '\0';
//             // cleanup();
            
//         }
//         usleep(100000); // 0.1초 대기

//     }

//     cleanup();

//     MQTTClient_disconnect(client, TIMEOUT);
//     MQTTClient_destroy(&client);
//     return 0;
// }