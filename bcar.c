#include <stdio.h>                // 표준 입출력
#include <stdlib.h>               // 메모리 할당, 변환 함수 등
#include <string.h>               // 문자열 처리 함수
#include <unistd.h>               // usleep 함수
#include "MQTTClient.h"           // Paho MQTT C 클라이언트

// 방향 정의
#define NORTH   0    // 북쪽
#define EAST    1    // 동쪽
#define SOUTH   2    // 남쪽
#define WEST    3    // 서쪽

// MQTT 설정
#define ADDRESS   "tcp://localhost:1883"  // 브로커 주소
#define CLIENTID  "Vehicle_B"             // 클라이언트 ID
#define TOPIC_PUB "vehicle/status_B"      // 상태 발행 토픽
#define TOPIC_SUB "vehicle/storage/B"     // 제어 명령 구독 토픽
#define QOS       0                         // QoS 레벨
#define TIMEOUT   10000L                    // 연결 타임아웃 (ms)

// 그리드 및 경로 설정
#define ROWS      7     // 행 개수
#define COLS      9     // 열 개수
#define MAX_PATH  100   // 최대 경로 길이

// 점 좌표 구조체
typedef struct { int r, c; } Point;

// A* 노드 구조체
typedef struct Node {
    Point pt;           // 현재 위치
    int g, h, f;        // g: 시작->현재, h: 휴리스틱, f=g+h
    struct Node *parent; // 부모 노드 포인터
} Node;

// 전역 변수 선언
static MQTTClient client;               // MQTT 클라이언트 핸들
static int grid[ROWS][COLS] = {         // 맵: 0=이동, 1=장애물, 문자=특수 지점
    {'A',0,0,0,0,0,0,0,0},
    {0,1,1,1,0,1,1,1,0},
    {0,1,'S',1,0,1,'G',1,0},
    {0,0,0,0,0,0,0,0,0},
    {0,1,1,1,0,1,1,1,0},
    {0,1,'K',1,0,1,'W',1,0},
    {0,0,0,0,0,0,0,0,'B'}
};
static Point path[MAX_PATH];           // 계산된 경로 저장
static int   path_len    = 0;          // 경로 길이
static int   path_idx    = 0;          // 경로 인덱스
static Point current_pos = {6, 8};     // B 차량 초기 위치
static int   dirB        = NORTH;      // B 차량 초기 방향
static volatile int move_permission = 0; // 이동 허가 플래그
static volatile int is_waiting       = 0; // 일시정지 플래그
static volatile int need_replan      = 0; // 재탐색 플래그
static char  goalsB[]    = {'W','B','S','B','G','B','K','B'}; // 목표 순서

// 함수 프로토타입
Point  find_point_by_char(char ch);
int    heuristic(Point a, Point b);
int    is_valid(int r, int c);
int    points_equal(Point a, Point b);
Node  *find_lowest_f(Node **open_set, int count);
int    in_set(Node **set, int count, Point pt);
void   reconstruct_path(Node *curr);
int    astar(Point start, Point goal);
void   publish_status(Point *path, int idx, int len);
void   print_grid_with_dir(Point pos, int dir);
int    msgarrvd(void *ctx, char *topic, int len, MQTTClient_message *message);

// 휴리스틱: 맨해튼 거리
int heuristic(Point a, Point b) {
    return abs(a.r - b.r) + abs(a.c - b.c);
}

// 유효한 셀 확인
int is_valid(int r, int c) {
    return (r>=0 && r<ROWS && c>=0 && c<COLS && grid[r][c] != 1);
}

// 좌표 동일 여부 확인
int points_equal(Point a, Point b) {
    return (a.r == b.r && a.c == b.c);
}

// open_set 중 최소 f 노드 선택
Node *find_lowest_f(Node **open_set, int count) {
    Node *best = open_set[0];
    for (int i = 1; i < count; i++) {
        if (open_set[i]->f < best->f) best = open_set[i];
    }
    return best;
}

// 배열 내 좌표 존재 검사
int in_set(Node **set, int count, Point pt) {
    for (int i = 0; i < count; i++) {
        if (points_equal(set[i]->pt, pt)) return i;
    }
    return -1;
}

// 경로 복원
void reconstruct_path(Node *curr) {
    path_len = 0;
    while (curr && path_len < MAX_PATH) {
        path[path_len++] = curr->pt;
        curr = curr->parent;
    }
    // 경로 역순 뒤집기
    for (int i = 0; i < path_len/2; i++) {
        Point tmp = path[i];
        path[i] = path[path_len-1-i];
        path[path_len-1-i] = tmp;
    }
}

// A* 경로 탐색 구현
int astar(Point start, Point goal) {
    Node *open_set[ROWS*COLS]; int open_count = 0;
    Node *closed_set[ROWS*COLS]; int closed_count = 0;

    Node *sn = malloc(sizeof(Node));
    sn->pt     = start;
    sn->g      = 0;
    sn->h      = heuristic(start, goal);
    sn->f      = sn->g + sn->h;
    sn->parent = NULL;
    open_set[open_count++] = sn;

    while (open_count > 0) {
        Node *curr = find_lowest_f(open_set, open_count);
        if (points_equal(curr->pt, goal)) {
            reconstruct_path(curr);
            break;
        }
        int idx = in_set(open_set, open_count, curr->pt);
        open_set[idx] = open_set[--open_count];
        closed_set[closed_count++] = curr;

        int dr[4] = {-1,1,0,0}, dc[4] = {0,0,-1,1};
        for (int i = 0; i < 4; i++) {
            Point np = { curr->pt.r + dr[i], curr->pt.c + dc[i] };
            if (!is_valid(np.r, np.c)) continue;
            if (in_set(closed_set, closed_count, np) != -1) continue;

            int ng = curr->g + 1;
            int found = in_set(open_set, open_count, np);
            if (found == -1) {
                Node *nn = malloc(sizeof(Node));
                nn->pt     = np;
                nn->g      = ng;
                nn->h      = heuristic(np, goal);
                nn->f      = nn->g + nn->h;
                nn->parent = curr;
                open_set[open_count++] = nn;
            } else if (ng < open_set[found]->g) {
                open_set[found]->g      = ng;
                open_set[found]->f      = ng + open_set[found]->h;
                open_set[found]->parent = curr;
            }
        }
    }

    for (int i = 0; i < open_count; i++)   free(open_set[i]);
    for (int i = 0; i < closed_count; i++) free(closed_set[i]);

    return (path_len > 0);
}

// 상태 발행 함수 (방향 로직 추가)
void publish_status(Point *path, int idx, int len) {
    char payload[256], pts[128] = "";
    int cnt = 0;
    for (int i = idx; i < idx+4 && i < len; i++, cnt++) {
        sprintf(pts+strlen(pts), "(%d,%d)%s", path[i].r, path[i].c, (i<idx+3)?",":"");
    }
    // 포맷: POS: (r,c) PATH: [..]
    sprintf(payload, "POS: (%d,%d) PATH: [%s]", current_pos.r, current_pos.c, pts);
    printf("[송신] B -> %s\n", payload);

    MQTTClient_message msg = MQTTClient_message_initializer;
    msg.payload    = payload;
    msg.payloadlen = strlen(payload);
    msg.qos        = QOS;
    msg.retained   = 0;
    MQTTClient_publishMessage(client, TOPIC_PUB, &msg, NULL);
}

// 그리드 및 방향 출력
void print_grid_with_dir(Point pos, int dir) {
    char arr[4] = {'^','>','v','<'};
    printf("   "); for (int c=0; c<COLS; c++) printf("%d ",c); puts("");
    for (int r=0; r<ROWS; r++) {
        printf("%d: ",r);
        for (int c=0; c<COLS; c++) {
            if (r==pos.r && c==pos.c) printf("%c ", arr[dir]);
            else if (grid[r][c]==1)    printf("# ");
            else                        printf(". ");
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

// 콜백 처리
int msgarrvd(void *context, char *topic, int tlen, MQTTClient_message *message) {
    char buf[message->payloadlen+1];
    memcpy(buf, message->payload, message->payloadlen);
    buf[message->payloadlen]='\0';
    printf("[수신] %s\n", buf);
    if (!strcmp(buf,"move")) {
        is_waiting=0; move_permission=1; puts(">> move");
    } else if (!strcmp(buf,"wait")) {
        is_waiting=1; puts(">> wait");
    } else if (!strcmp(buf,"replan")) {
        need_replan=1; puts(">> replan");
    }
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topic);
    return 1;
}

int main(void) {
    MQTTClient_connectOptions opts=MQTTClient_connectOptions_initializer;
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client,NULL,NULL,msgarrvd,NULL);
    if (MQTTClient_connect(client,&opts)!=MQTTCLIENT_SUCCESS) {
        fprintf(stderr,"MQTT 연결 실패\n"); return 1; }
    MQTTClient_subscribe(client,TOPIC_SUB,QOS);

    int n = sizeof(goalsB)/sizeof(goalsB[0]);
    for (int i=0; i<n; i++) {
        Point g = find_point_by_char(goalsB[i]);
        if (!astar(current_pos,g)) { printf("경로 탐색 실패: %c\n",goalsB[i]); continue; }
        path_idx=0; publish_status(path,path_idx,path_len);
        while(path_idx<path_len) {
            while(is_waiting||!move_permission) { MQTTClient_yield(); usleep(100000); }
            move_permission=0;

            // 방향 제어 및 이동
            Point nxt = path[path_idx];
            int td = (nxt.r<current_pos.r?NORTH: nxt.r>current_pos.r?SOUTH:
                      nxt.c>current_pos.c?EAST:WEST);
            int diff = (td - dirB + 4) % 4; if(diff==3) diff=-1;
            if(diff<0) { puts("[B] TURN_LEFT"); dirB=(dirB+diff+4)%4; }
            else if(diff>0) { puts("[B] TURN_RIGHT"); dirB=(dirB+diff)%4; }
            else { puts("[B] FORWARD"); current_pos=nxt; path_idx++; }

            publish_status(path,path_idx,path_len);
            print_grid_with_dir(current_pos, dirB);
        }
    }
    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);
    return 0;
}
