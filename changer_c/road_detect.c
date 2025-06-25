//road_detect.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <MQTTClient.h>

// #define ADDRESS "ws://mqtt.choidaruhan.xyz:8083"
#define ADDRESS "tcp://broker.hivemq.com:1883"
#define CLIENTID "Storage"
#define QOS 0
#define TIMEOUT 10000L

#define TOPIC_A "vehicle/status_A"
#define TOPIC_B "vehicle/status_B"
#define CMD_A "vehicle/storage/A"
#define CMD_B "vehicle/storage/B"


#define MAX_PATH 20
#define ROWS 7
#define COLS 9

typedef struct {
    int x, y;
    int path[MAX_PATH][2];
    int path_len;
    int path_index; // 현재 경로 인덱스
} VehicleState;

VehicleState vehicleA = {-1, -1, {{-1, -1}}, 0};
VehicleState vehicleB = {-1, -1, {{-1, -1}}, 0};

MQTTClient client;

int conflict_detected = 0;
int hold_index = -1;
int A_hold_state = 0;
int has_A = 0, has_B = 0;

void parse_vehicle_message(char *msg, VehicleState *vehicle) {
    int idx = 0;
    char *ptr = strstr(msg, "POS: (");
    if (ptr)
        sscanf(ptr, "POS: (%d,%d)", &vehicle->x, &vehicle->y);

    ptr = strstr(msg, "PATH: [");
    if (ptr) {
        vehicle->path_len = 0;
        ptr += strlen("PATH: [");
        while (*ptr && *ptr != ']') {
            int x, y;
            if (sscanf(ptr, "(%d,%d)", &x, &y) == 2 && vehicle->path_len < MAX_PATH) {
                vehicle->path[vehicle->path_len][0] = x;
                vehicle->path[vehicle->path_len][1] = y;
                vehicle->path_len++;
            }
            ptr = strchr(ptr, ')');
            if (ptr) ptr++;
            while (*ptr == ',' || *ptr == ' ') ptr++;
        }
    }
    ptr = strstr(msg, "INDEX:");
    if (ptr) {
        sscanf(ptr, "INDEX: %d", &vehicle->path_index);
    } else {
        vehicle->path_index = 0;  // 기본값 초기화
    }
}

int detect_conflict_index(const VehicleState *a, const VehicleState *b) {
    if (a->path_len == 0 || b->path_len == 0) return -1; // 같은 좌표를 동시에 가는 경우 

    for (int i = 0; i < a->path_len; i++) {
        int ax = a->path[i][0], ay = a->path[i][1];
        for (int j = 0; j < b->path_len; j++) {
            int bx = b->path[j][0], by = b->path[j][1];
            if (ax == bx && ay == by)
                return i; // 단순히 A의 경로 상 겹치는 좌표 발견 시 index 반환
        }
    }
    return -1;
}

void print_positions() {
    char grid[ROWS][COLS];
    for (int i = 0; i < ROWS; i++)
        for (int j = 0; j < COLS; j++)
            grid[i][j] = '.';

    if (vehicleA.x >= 0 && vehicleA.x < ROWS && vehicleA.y >= 0 && vehicleA.y < COLS)
        grid[vehicleA.x][vehicleA.y] = 'A';

    if (vehicleB.x >= 0 && vehicleB.x < ROWS && vehicleB.y >= 0 && vehicleB.y < COLS) {
        if (grid[vehicleB.x][vehicleB.y] == 'A')
            grid[vehicleB.x][vehicleB.y] = 'X';
        else
            grid[vehicleB.x][vehicleB.y] = 'B';
    }

    printf("\n📍 현재 차량 위치:\n");
    for (int i = 0; i < ROWS; i++) {
        for (int j = 0; j < COLS; j++)
            printf("%c ", grid[i][j]);
        printf("\n");
    }
    printf("\n");
}

void send_message(const char *topic, const char *msg) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void *)msg;
    pubmsg.payloadlen = (int)strlen(msg);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;
    MQTTClient_publishMessage(client, topic, &pubmsg, NULL);
}

void evaluate_conflict_and_command() {
    if (has_A && !has_B) {
        if (A_hold_state) {
            // A가 이전에 HOLD 상태였으면 대기 유지
            printf("⏸️ B 좌표 없음 + A HOLD 상태 유지\n");
        } 
        else if (vehicleA.path_len > 0 && vehicleA.path_index < vehicleA.path_len) {
            send_message(CMD_A, "move");
            printf("ℹ️ B 좌표 없음 → A move 명령\n");
        } else {
            printf("🚫 A 차량 경로 없음 또는 도착 상태 → move 생략\n");
        }
        return;
    }

    if (!has_A && has_B) {
        send_message(CMD_B, "move");
        printf("ℹ️ A 좌표 없음 → B move 명령\n");
        return;
    }

    hold_index = detect_conflict_index(&vehicleA, &vehicleB);

    if (hold_index != -1) {
        conflict_detected = 1;

        int target_index = hold_index > 1 ? hold_index - 2 : hold_index - 1;
        if (target_index >= 0) {
            int hx = vehicleA.path[target_index][0];
            int hy = vehicleA.path[target_index][1];

            if (vehicleA.x == hx && vehicleA.y == hy && !A_hold_state) {
                send_message(CMD_A, "hold");
                printf("⚠️ A 차량 HOLD (충돌 index: %d → (%d,%d))\n", hold_index, hx, hy);
                A_hold_state = 1;
            }
        }

        send_message(CMD_B, "move");
    } 
    else {
        if (A_hold_state) {
            send_message(CMD_A, "move");
            printf("a차 move 송신");
            A_hold_state = 0;
        }
        else{
            send_message(CMD_A, "move");
            A_hold_state = 0;
        }
        send_message(CMD_B, "move");
        conflict_detected = 0;
        printf("✅ 충돌 없음 → A, B move\n");
    }
}

int message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char *payload = (char *)message->payload;
    printf("📥 수신 토픽: %s\n", topicName);
    printf("📥 수신 메시지: %.*s\n", message->payloadlen, payload);

    
    if (strcmp(topicName, TOPIC_A) == 0) {
        parse_vehicle_message(payload, &vehicleA);
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                "python3 - << 'EOF'\n"
                "from db_access import get_connection,update_vehicle_coords\n"
                "conn = get_connection()\n"
                "cur = conn.cursor()\n"
                "update_vehicle_coords(cur, conn, %d, %d,\"%s\")\n"
                "conn.close()\n"
                "EOF",
                vehicleA.x,
                vehicleA.y,
                "A-1000" // 기존 운행_ID
        );
        system(cmd);

        print_positions();
        has_A = 1;
        evaluate_conflict_and_command();
    }
    else if (strcmp(topicName, TOPIC_B) == 0) {
        parse_vehicle_message(payload, &vehicleB);
        char x_str[12];
        char y_str[12];

        snprintf(x_str, sizeof(x_str), "%d", vehicleB.x);
        snprintf(y_str, sizeof(y_str), "%d", vehicleB.y);
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                "python3 - << 'EOF'\n"
                "from db_access import get_connection,update_vehicle_coords\n"
                "conn = get_connection()\n"
                "cur = conn.cursor()\n"
                "update_vehicle_coords(cur, conn, %s, %s,\"%s\")\n"
                "conn.close()\n"
                "EOF",
                x_str,
                y_str,
                "B-1001" // 기존 운행_ID
        );
        system(cmd);

        print_positions();
        has_B = 1;
        evaluate_conflict_and_command();
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

int main() {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, NULL, message_arrived, NULL);

    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        printf("❌ MQTT 연결 실패\n");
        return -1;
    }

    MQTTClient_subscribe(client, TOPIC_A, QOS);
    MQTTClient_subscribe(client, TOPIC_B, QOS);

    printf("📦 보관함 충돌 감지 시스템 작동 시작...\n");

    while (1) {
        sleep(1);
    }

    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);
    return 0;
}