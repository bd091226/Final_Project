// 상단은 동일
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "MQTTClient.h"

#define ADDRESS "tcp://localhost:1883"
#define CLIENTID "Vehicle_A"
#define TOPIC_PUB "vehicle/status"
#define TOPIC_SUB "vehicle/storage/A"
#define QOS 0
#define TIMEOUT 10000L

#define ROWS 9
#define COLS 9
#define MAX_PATH 100

#define ID "A"

int grid[ROWS][COLS] = {
    {'B', 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 0, 1, 1, 1, 0},
    {0, 1, 1, 1, 0, 1, 1, 1, 0},
    {0, 1, 'S', 1, 0, 1, 'G', 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 1, 1, 1, 0, 1, 1, 1, 0},
    {0, 1, 1, 1, 0, 1, 1, 1, 0},
    {0, 1, 'K', 1, 0, 1, 'W', 1, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 'A'}};

typedef struct
{
    int r, c;
} Point;
typedef struct Node
{
    Point pt;
    int g, h, f;
    struct Node *parent;
} Node;

Point path[MAX_PATH];
int path_len = 0;
int path_idx = 0;

MQTTClient client;
char goalsA[] = {'K', 'G', 'S', 'W', 'A'};
Point current_pos = {8, 8};
volatile int move_permission = 0; // 수신된 move 플래그

int heuristic(Point a, Point b) { return abs(a.r - b.r) + abs(a.c - b.c); }

int is_valid(int r, int c)
{
    if (r < 0 || r >= ROWS || c < 0 || c >= COLS)
        return 0;
    if (grid[r][c] == 1)
        return 0;
    return 1;
}

int points_equal(Point a, Point b)
{
    return a.r == b.r && a.c == b.c;
}

Node *find_lowest_f(Node **open_set, int count)
{
    Node *lowest = open_set[0];
    for (int i = 1; i < count; i++)
    {
        if (open_set[i]->f < lowest->f)
            lowest = open_set[i];
    }
    return lowest;
}

int in_set(Node **set, int count, Point pt)
{
    for (int i = 0; i < count; i++)
    {
        if (points_equal(set[i]->pt, pt))
            return i;
    }
    return -1;
}

void reconstruct_path(Node *current)
{
    path_len = 0;
    // current = current->parent; // 시작점 제외
    //  목표 지점까지 포함하여 경로 추적
    while (current && path_len < MAX_PATH)
    {
        path[path_len++] = current->pt;
        current = current->parent;
    }
    for (int i = 0; i < path_len / 2; i++)
    {
        Point tmp = path[i];
        path[i] = path[path_len - 1 - i];
        path[path_len - 1 - i] = tmp;
    }
}

int astar(Point start, Point goal)
{
    Node *open_set[ROWS * COLS];
    int open_count = 0;
    Node *closed_set[ROWS * COLS];
    int closed_count = 0;

    Node *start_node = malloc(sizeof(Node));
    start_node->pt = start;
    start_node->g = 0;
    start_node->h = heuristic(start, goal);
    start_node->f = start_node->g + start_node->h;
    start_node->parent = NULL;
    open_set[open_count++] = start_node;

    while (open_count > 0)
    {
        Node *current = find_lowest_f(open_set, open_count);
        if (points_equal(current->pt, goal))
        {
            reconstruct_path(current);
            goto cleanup;
        }

        int cur_idx = -1;
        for (int i = 0; i < open_count; i++)
        {
            if (open_set[i] == current)
            {
                cur_idx = i;
                break;
            }
        }
        for (int i = cur_idx; i < open_count - 1; i++)
            open_set[i] = open_set[i + 1];
        open_count--;

        closed_set[closed_count++] = current;

        int dr[4] = {-1, 1, 0, 0}, dc[4] = {0, 0, -1, 1};
        for (int i = 0; i < 4; i++)
        {
            int nr = current->pt.r + dr[i], nc = current->pt.c + dc[i];
            if (!is_valid(nr, nc))
                continue;
            if (in_set(closed_set, closed_count, (Point){nr, nc}) != -1)
                continue;

            int tentative_g = current->g + 1;
            int open_idx = in_set(open_set, open_count, (Point){nr, nc});
            if (open_idx == -1)
            {
                Node *neighbor = malloc(sizeof(Node));
                neighbor->pt = (Point){nr, nc};
                neighbor->g = tentative_g;
                neighbor->h = heuristic(neighbor->pt, goal);
                neighbor->f = neighbor->g + neighbor->h;
                neighbor->parent = current;
                open_set[open_count++] = neighbor;
            }
            else if (tentative_g < open_set[open_idx]->g)
            {
                open_set[open_idx]->g = tentative_g;
                open_set[open_idx]->f = tentative_g + open_set[open_idx]->h;
                open_set[open_idx]->parent = current;
            }
        }
    }

cleanup:
    for (int i = 0; i < open_count; i++)
        free(open_set[i]);
    for (int i = 0; i < closed_count; i++)
        free(closed_set[i]);
    return path_len > 0;
}

// 현재 좌표를 송신하는 함수
// void publish_status(Point pos)
// {
//     char payload[128];
//     snprintf(payload, sizeof(payload),
//              "{\"id\":\"%s\",\"pos\":{\"r\":%d,\"c\":%d}}", ID, pos.r, pos.c);
//     printf("[송신] : A %d %d\n", pos.r, pos.c);

//     MQTTClient_message pubmsg = MQTTClient_message_initializer;
//     pubmsg.payload = payload;
//     pubmsg.payloadlen = (int)strlen(payload);
//     pubmsg.qos = QOS;
//     pubmsg.retained = 0;

//     MQTTClient_deliveryToken token;
//     MQTTClient_publishMessage(client, TOPIC_PUB, &pubmsg, &token);
//     // MQTTClient_waitForCompletion(client, token, TIMEOUT);
// }

// 현재 좌표 포함 다음 좌표 3개를 보내는 함수
void publish_multi_status(Point *path, int idx, int len)
{
    char payload[256];
    char positions[128] = "";

    int count = 0;
    for (int i = idx - 1; i <= len && count < 3; i++) // 현재 위치 포함 최대 3개
    {
        if (i < 0) // 처음 current_pos의 경우
        {
            snprintf(positions + strlen(positions), sizeof(positions) - strlen(positions),
                     "{\"r\":%d,\"c\":%d},", current_pos.r, current_pos.c);
        }
        else
        {
            snprintf(positions + strlen(positions), sizeof(positions) - strlen(positions),
                     "{\"r\":%d,\"c\":%d},", path[i].r, path[i].c);
        }
        count++;
    }

    // 마지막 콤마 제거
    if (positions[strlen(positions) - 1] == ',')
        positions[strlen(positions) - 1] = '\0';

    snprintf(payload, sizeof(payload),
             "{\"id\":\"%s\",\"path\":[%s]}", ID, positions);
    printf("[송신] : A path -> %s\n", payload);

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    MQTTClient_publishMessage(client, TOPIC_PUB, &pubmsg, &token);
}

Point find_point_by_char(char ch)
{
    for (int r = 0; r < ROWS; r++)
        for (int c = 0; c < COLS; c++)
            if (grid[r][c] == ch)
                return (Point){r, c};
    return (Point){-1, -1};
}

// 처음 시작 좌표만 송신
void publish_start_status(Point pos)
{
    char payload[128];
    snprintf(payload, sizeof(payload),
             "{\"id\":\"%s\",\"path\":[{\"r\":%d,\"c\":%d}]}", ID, pos.r, pos.c);
    printf("[송신] : A 시작 좌표 -> %s\n", payload);

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    MQTTClient_publishMessage(client, TOPIC_PUB, &pubmsg, &token);
}

void print_grid_with_A(Point Apos)
{
    printf("=== 지도 상태 ===\n");
    for (int r = 0; r < ROWS; r++)
    {
        for (int c = 0; c < COLS; c++)
        {
            if (r == Apos.r && c == Apos.c)
                printf("A ");
            else if (grid[r][c] == 1)
                printf("# ");
            else if (grid[r][c] == 0)
                printf(". ");
            else
                printf("%c ", grid[r][c]);
        }
        printf("\n");
    }
    printf("================\n");
}

// 수신
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    printf("수신 콜백 도착: topic=%s, len=%d\n", topicName, message->payloadlen);

    char msg[message->payloadlen + 1];
    memcpy(msg, message->payload, message->payloadlen);
    msg[message->payloadlen] = '\0';

    printf("수신한 메시지 내용: %s\n", msg);

    if (strcmp(msg, "move") == 0)
    {
        move_permission = 1;
        printf(">> move 명령 수신됨, 이동 시작\n");
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

int main()
{
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, NULL, msgarrvd, NULL);

    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "MQTT 브로커 연결 실패\n");
        return 1;
    }

    MQTTClient_subscribe(client, TOPIC_SUB, QOS);

    int goal_count = sizeof(goalsA) / sizeof(goalsA[0]);
    for (int goal_i = 0; goal_i < goal_count; goal_i++)
    {
        Point goal = find_point_by_char(goalsA[goal_i]);
        if (goal.r == -1)
        {
            printf("목표 '%c'를 찾을 수 없습니다.\n", goalsA[goal_i]);
            continue;
        }

        if (!astar(current_pos, goal))
        {
            printf("목표 '%c'로 가는 경로를 찾을 수 없습니다.\n", goalsA[goal_i]);
            continue;
        }

        path_idx = 0;
        // 처음 시작 좌표만 송신 (중복 방지)
        publish_start_status(current_pos);
        // print_grid_with_A(current_pos);

        while (path_idx < path_len)
        {
            // move_permission 이 1이 될 때까지 기다림
            while (!move_permission)
            {
                MQTTClient_yield();
                usleep(100 * 1000); // 0.1초 대기
            }

            move_permission = 0; // 플래그 초기화

            // 다음 좌표로 이동 및 송신
            current_pos = path[path_idx++];
            publish_multi_status(path, path_idx, path_len);
            // print_grid_with_A(current_pos);
        }
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return 0;
}
