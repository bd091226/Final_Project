#include <stdio.h>                // 표준 입출력 헤더
#include <stdlib.h>               // 메모리 할당, 변환 함수 등
#include <string.h>               // 문자열 처리 함수
#include <unistd.h>               // usleep 함수
#include <signal.h>               // SIGINT 처리
#include <MQTTClient.h>           // Paho MQTT C 클라이언트
#include <gpiod.h>
#include <sys/types.h>            // select, fd_set 사용
#include <sys/select.h>           // select 함수 사용
#include <ctype.h>

#include "acar.h"
#include "encoder.h"

// 전역 변수 정의
//MQTTClient client;

volatile int has_new_goal = 0;
volatile int move_permission = 0;
volatile int is_waiting = 0;

char current_goal_char = '\0';
char last_goal_char = '\0';

int grid[ROWS][COLS] = {
    {'A',0,0,0,0,0,0,0,0},
    {0,1,1,1,0,1,1,1,0},
    {0,1,'S',1,0,1,'G',1,0},
    {0,0,0,0,0,0,0,0,0},
    {0,1,1,1,0,1,1,1,0},
    {0,1,'K',1,0,1,'W',1,0},
    {0,0,0,0,0,0,0,0,'B'}
};

Point path[MAX_PATH];
int path_len = 0;
int path_idx = 0;
Point current_pos = {0, 0};
int dirA = SOUTH;


// 휴리스틱: 맨해튼 거리
int heuristic(Point a, Point b) 
{
    return abs(a.r - b.r) + abs(a.c - b.c);
}

// 셀 유효성 검사
int is_valid(int r, int c) 
{
    return (r >= 0 && r < ROWS && c >= 0 && c < COLS && grid[r][c] != 1);
}

// 좌표 비교
int points_equal(Point a, Point b) 
{
    return (a.r == b.r && a.c == b.c);
}

// 최소 f 값 노드 선택
Node *find_lowest_f(Node **open_set, int count) 
{
    Node *best = open_set[0];
    for (int i = 1; i < count; i++)
        if (open_set[i]->f < best->f)
            best = open_set[i];
    return best;
}

// 집합 내 좌표 존재 확인
int in_set(Node **set, int count, Point pt) 
{
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
    while (curr && len < MAX_PATH) 
    {
        tmp[len++] = curr->pt;
        curr = curr->parent;
    }
    // 경로가 없으면 종료
    if (len == 0)
    {
        path_len = 0;
        return;
    }
    // 시작 위치(tmp[len-1])를 제외한 실제 이동 경로를 순서대로 복원
    path_len = len - 1;
    for (int i = 0; i < path_len; i++) 
    {
        path[i] = tmp[len - 2 - i];
    }
}

int astar(Point start, Point goal) 
{
    Node *open_set[ROWS*COLS]; int oc=0;
    Node *closed_set[ROWS*COLS]; int cc=0;
    Node *sn = malloc(sizeof(Node));
    sn->pt = start;
    sn->g = 0;
    sn->h = heuristic(start, goal);
    sn->f = sn->g + sn->h;
    sn->parent = NULL;
    open_set[oc++] = sn;

    while (oc > 0) 
    {
        Node *curr = find_lowest_f(open_set, oc);
        if (points_equal(curr->pt, goal)) 
        {
            reconstruct_path(curr);
            break;
        }
        int idx = in_set(open_set, oc, curr->pt);
        open_set[idx] = open_set[--oc];
        closed_set[cc++] = curr;

        int dr[4] = {-1,1,0,0}, dc[4] = {0,0,-1,1};
        for (int i = 0; i < 4; i++) 
        {
            Point np = { curr->pt.r + dr[i], curr->pt.c + dc[i] };
            if (!is_valid(np.r,np.c) || in_set(closed_set,cc,np) != -1)
                continue;

            int ng = curr->g + 1;
            int fi = in_set(open_set, oc, np);
            if (fi < 0) 
            {
                Node *nn = malloc(sizeof(Node));
                nn->pt = np;
                nn->g = ng;
                nn->h = heuristic(np, goal);
                nn->f = nn->g + nn->h;
                nn->parent = curr;
                open_set[oc++] = nn;
            } 
            else if (ng < open_set[fi]->g) 
            {
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
    for (int i = idx-1; i <= len && count<4; i++,count++) 
    {
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

void print_grid_with_dir(Point pos, int dir) 
{
    char arrow[4] = {'^','>','v','<'};
    printf("   "); for (int c=0; c<COLS; c++) printf("%d ", c); puts("");
    for (int r=0; r<ROWS; r++) {
        printf("%d: ", r);
        for (int c=0; c<COLS; c++) 
        {
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

Point find_point_by_char(char ch) 
{
    for (int r=0; r<ROWS; r++)
        for (int c=0; c<COLS; c++)
            if (grid[r][c]==ch)
                return (Point){r,c};
    return (Point){-1,-1};
}

int publish_message_a(const char* topic, const char* payload) 
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void*)payload;
    pubmsg.payloadlen = strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) 
    {
        printf("[오류] 메시지 발행 실패: %d\n", rc);
        return rc;
    }

    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    return rc;
}