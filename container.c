#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <MQTTClient.h>

#define ADDRESS "tcp://localhost:1883"
#define CLIENTID "Storage"
#define QOS 0
#define TIMEOUT 10000L

#define TOPIC_A "vehicle/status_A"
#define TOPIC_B "vehicle/status_B"
#define CMD_A "vehicle/storage/A"
#define CMD_B "vehicle/storage/B"

#define MAX_PATH 20
#define GRID_SIZE 9

typedef struct
{
    int x, y;
    int path[MAX_PATH][2];
    int path_len;
} VehicleState;

VehicleState vehicleA = {-1, -1, {{-1, -1}}, 0};
VehicleState vehicleB = {-1, -1, {{-1, -1}}, 0};

// 이전 상태 저장
VehicleState prevA = {-1, -1, {{-1, -1}}, 0};

MQTTClient client;
int conflict_detected = 0;
int hold_index = -1;

void parse_vehicle_message(char *msg, VehicleState *vehicle)
{
    int idx = 0;
    char *ptr = strstr(msg, "POS: (");
    if (ptr)
        sscanf(ptr, "POS: (%d,%d)", &vehicle->x, &vehicle->y);

    ptr = strstr(msg, "PATH: [");
    if (ptr)
    {
        vehicle->path_len = 0;
        ptr += strlen("PATH: [");
        while (*ptr && *ptr != ']')
        {
            int x, y;
            if (sscanf(ptr, "(%d,%d)", &x, &y) == 2 && vehicle->path_len < MAX_PATH)
            {
                vehicle->path[vehicle->path_len][0] = x;
                vehicle->path[vehicle->path_len][1] = y;
                vehicle->path_len++;
            }
            ptr = strchr(ptr, ')');
            if (ptr)
                ptr++;
            while (*ptr == ',' || *ptr == ' ')
                ptr++;
        }
    }
}

int detect_conflict_index(const VehicleState *a, const VehicleState *b)
{
    if (a->path_len == 0 || b->path_len == 0)
        return -1;

    int len = a->path_len < b->path_len ? a->path_len : b->path_len;
    for (int i = 0; i < len; i++)
    {
        int ax = a->path[i][0], ay = a->path[i][1];
        int bx = b->path[i][0], by = b->path[i][1];
        if (ax == bx && ay == by)
            return i;
        if (i > 0 &&
            a->path[i - 1][0] == bx && a->path[i - 1][1] == by &&
            b->path[i - 1][0] == ax && b->path[i - 1][1] == ay)
            return i;
    }
    return -1;
}

void print_positions()
{
    char grid[GRID_SIZE][GRID_SIZE];
    for (int i = 0; i < GRID_SIZE; i++)
        for (int j = 0; j < GRID_SIZE; j++)
            grid[i][j] = '.';

    if (vehicleA.x >= 0 && vehicleA.x < GRID_SIZE && vehicleA.y >= 0 && vehicleA.y < GRID_SIZE)
        grid[vehicleA.x][vehicleA.y] = 'A';

    if (vehicleB.x >= 0 && vehicleB.x < GRID_SIZE && vehicleB.y >= 0 && vehicleB.y < GRID_SIZE)
    {
        if (grid[vehicleB.x][vehicleB.y] == 'A')
            grid[vehicleB.x][vehicleB.y] = 'X';
        else
            grid[vehicleB.x][vehicleB.y] = 'B';
    }

    printf("\n📍 현재 차량 위치:\n");
    for (int i = 0; i < GRID_SIZE; i++)
    {
        for (int j = 0; j < GRID_SIZE; j++)
            printf("%c ", grid[i][j]);
        printf("\n");
    }
    printf("\n");
}

void send_message(const char *topic, const char *msg)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void *)msg;
    pubmsg.payloadlen = (int)strlen(msg);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    MQTTClient_publishMessage(client, topic, &pubmsg, NULL);
}

int message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    char *payload = (char *)message->payload;
    printf("📥 수신 토픽: %s\n", topicName);
    printf("📥 수신 메시지: %.*s\n", message->payloadlen, payload);

    if (strcmp(topicName, TOPIC_A) == 0)
    {
        parse_vehicle_message(payload, &vehicleA);
        print_positions();

        if (!conflict_detected)
        {
            memcpy(&prevA, &vehicleA, sizeof(VehicleState));
        }
    }
    else if (strcmp(topicName, TOPIC_B) == 0)
    {
        parse_vehicle_message(payload, &vehicleB);
        print_positions();

        hold_index = detect_conflict_index(&prevA, &vehicleB);

        if (hold_index != -1)
        {
            conflict_detected = 1;

            int target_index = hold_index > 1 ? hold_index - 2 : hold_index - 1;
            if (target_index >= 0)
            {
                int hx = prevA.path[target_index][0];
                int hy = prevA.path[target_index][1];

                if (vehicleA.x == hx && vehicleA.y == hy)
                {
                    send_message(CMD_A, "HOLD: wait 2000ms");
                    printf("⚠️ 차량 A에게 사전 정지 명령 (충돌 index: %d → 좌표: (%d,%d))\n", hold_index, hx, hy);
                }
            }

            send_message(CMD_B, "move");
        }
        else
        {
            conflict_detected = 0;
            printf("✅ 충돌 없음 → 차량 A, B 모두 move\n");
            send_message(CMD_A, "move");
            send_message(CMD_B, "move");
        }
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

int main()
{
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, NULL, message_arrived, NULL);

    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS)
    {
        printf("❌ MQTT 연결 실패\n");
        return -1;
    }

    MQTTClient_subscribe(client, TOPIC_A, QOS);
    MQTTClient_subscribe(client, TOPIC_B, QOS);

    printf("📦 보관함 충돌 감지 시스템 작동 시작...\n");

    while (1)
    {
        sleep(1);
    }

    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);
    return 0;
}
