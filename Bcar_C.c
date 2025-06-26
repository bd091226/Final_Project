/*
컴파일 :
gcc -g Bcar_C.c Bcar_moter.c moter_control.c encoder.c -o Bcar_C -lpaho-mqtt3c -lgpiod
실행 :
./Bcar_C

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <MQTTClient.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <pthread.h>
#include <ctype.h>
#include <math.h>  // fabs
#include <cjson/cJSON.h>  // 반드시 포함 필요
#include "moter_control.h"
#include "Bcar_moter.h"
#include "encoder.h"

#define ADDRESS "tcp://broker.hivemq.com:1883"
// #define CLIENTID "RaspberryPi_Bcar"
#define TOPIC_B_DANGER       "vehicle/emergency/B"
#define TOPIC_B_DEST "storage/b_dest"
#define TOPIC_B_DEST_ARRIVED "storage/b_dest_arrived"
#define TOPIC_B_POINT_ARRIVED "storage/b_point_arrived"
#define TOPIC_B_POINT        "storage/b_point"
#define TOPIC_B_COMPLETED "vehicle/B_completed"
#define TOPIC_B_QR "storage/gr_B"

#define PYTHON_SCRIPT_PATH  "/home/pi/Final_Project/aruco_stream.py"
#define PYTHON_BIN          "python3"

volatile int danger_detected = 0; // 긴급 호출 감지 플래그
volatile int resume_button_pressed = 0;
bool is_emergency_return = false;

// 최신 오류값 저장
float latest_tvec[3] = {0};
float latest_rvec_yaw = 0;
bool need_correction = false;
pid_t python_pid = -1;   // 파이썬 프로세스 PID 저장


// Python 스크립트를 새 프로세스로 실행
void start_python_script() {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return;
    }
    if (pid == 0) {
        // 자식 프로세스: execlp로 파이썬 스크립트 실행
        execlp(PYTHON_BIN, PYTHON_BIN, PYTHON_SCRIPT_PATH, (char*)NULL);
        // execlp가 실패하면 아래 코드가 실행됨
        perror("execlp failed");
        exit(EXIT_FAILURE);
    }
    // 부모 프로세스: child PID 저장
    python_pid = pid;
    printf("[INFO] Started Python script (PID=%d)\n", python_pid);
}

// --- 거리(cm) -> 이동 시간(sec) 환산 함수 (예: 10cm당 0.3초) ---
float distance_to_time(float cm) {
    const float time_per_cm = 0.003;  // 실험으로 조정 필요
    return cm * time_per_cm;
}

// --- 각도(rad) -> 회전 시간(sec) 환산 함수 (예: 90도(1.57rad) 당 0.5초) ---
float angle_to_time(float rad) {
    const float time_per_rad = 0.16; // 실험으로 조정 필요 (0.5초 / 1.57rad)
    return fabs(rad) * time_per_rad;
}
// --- ArUco 마커 기반 보정 루틴 ---
void correct_position_from_aruco(float tvec[3], float yaw) {
    float error_x = tvec[0];    // cm 단위
    float error_z = tvec[2];    // cm 단위
    float angle_rad = yaw;      // rad 단위

    printf("📍 보정 시작: X=%.2fcm, Z=%.2fcm, Yaw=%.3frad\n", error_x, error_z, angle_rad);

    // 1. 방향 보정 (Yaw)
    if (fabs(angle_rad) > 2.0) {
        float rotate_time = angle_to_time(angle_rad);
        if (angle_rad > 0) {
            printf("↩️ 좌회전 보정: %.2f초\n", rotate_time);
            rotate_left_time(rotate_time);
        } else {
            printf("↪️ 우회전 보정: %.2f초\n", rotate_time);
            rotate_right_time(rotate_time);
        }
    }

    // 2. 좌우 중심 보정 (X축)
    if (fabs(error_x) > 2.0) {
        float move_time = distance_to_time(fabs(error_x));
        if (error_x > 0) {
            printf("↪️ 오른쪽으로 보정 이동: %.2f초\n", move_time);
            aruco_right_time(move_time);
        } else {
            printf("↩️ 왼쪽으로 보정 이동: %.2f초\n", move_time);
            aruco_left_time(move_time);
        }
    }

    // 3. 전방 거리 보정 (Z축)
    // if (error_z > 30.0) { // 25cm 이상이면 앞으로 이동
    //     float forward_time = distance_to_time(error_z - 20.0); // 20cm 거리 유지
    //     printf("⬆️ 앞으로 보정 이동: %.2f초\n", forward_time);
    //     aruco_forward_time(forward_time);
    // }

    motor_stop();
    printf("✅ 보정 완료\n");
}
// B차 출발지점 도착
void starthome()
{
    // MQTT 발행
    char payload[64];
    // 페이로드 버퍼에 목적지 구역 ID를 복사
    snprintf(payload, sizeof(payload), "출발지점도착");

    // 메시지 초기화
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;

    int rc = MQTTClient_publishMessage(client, TOPIC_B_POINT_ARRIVED, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "도착 메시지 발행 실패, rc=%d\n", rc);
        return;
    }
    printf("[송신] %s → %s\n", payload, TOPIC_B_POINT_ARRIVED);  // B지점 도착 알림 송신
}
// 메시지 송신
void send_arrival(const char *zone_id)
{

    // MQTT 발행
    char payload[64];
    // 페이로드 버퍼에 목적지 구역 ID를 복사
    snprintf(payload, sizeof(payload), "%s", zone_id);

    // 메시지 초기화
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;

    int rc = MQTTClient_publishMessage(client, TOPIC_B_POINT_ARRIVED, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "도착 메시지 발행 실패, rc=%d\n", rc);
        return;
    }
    // MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("[송신] %s → %s\n", payload, TOPIC_B_POINT_ARRIVED);
    sleep(3);
    starthome();
}

// 메시지 수신
int message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    cJSON *root = NULL;
    char msg[message->payloadlen + 1];
    memcpy(msg, message->payload, message->payloadlen);
    msg[message->payloadlen] = '\0';

    printf("[수신] 토픽: %s ->  %s\n", topicName, msg);

    // if (strcmp(topicName, TOPIC_B_DEST) == 0)
    // {
    //     sleep(2); // 출발 후 도착까지 딜레이
    //     send_arrival(msg);
    // }
    if(strcmp(topicName, TOPIC_B_POINT) == 0)
    {
        forward_one(&current_pos, dirB);
        rotate_one(&dirB, -1);
        // 오른쪽으로 회전
        starthome();
    }
    if (!strcmp(msg, "move")) 
    {
        is_waiting = 0; 
        move_permission = 1; 
        puts(">> move");
    }
    if (strcmp(topicName, TOPIC_B_DEST) == 0) 
    {
        // 문자열이 비어 있지 않고, 첫 글자가 대문자 알파벳이면 허용
        if (msg[0] == 'K' || msg[0] == 'G' || msg[0] == 'W' || msg[0] == 'S'|| msg[0] == 'B') {
            current_goal = msg[0];
            new_goal_received = 1;
            printf("➡️  A* 경로 탐색 시작: 목적지 '%c'\n", current_goal);
        } else {
            // 유효하지 않음 → 아무 반응 없이 무시
            printf("⚠️  무시됨: 잘못된 메시지 '%s'\n", msg);
        }
    }
    if(strcmp(topicName, TOPIC_B_DEST_ARRIVED) == 0) 
    {
        printf("도착지점 도착: %s\n", msg);
        send_arrival(msg);
    }
    if(strcmp(topicName, TOPIC_B_DANGER) == 0)
    {
        printf("긴급 호출 감지\n");
        danger_detected = 1;
    }
    if (strcmp(topicName, "storage/gr_B") == 0) 
    {
        // JSON 파싱
        cJSON *root = cJSON_Parse(msg);
        if (root == NULL) 
        {
            printf("⚠️ JSON 파싱 실패: %s\n", msg);
        } 
        else 
        {
            cJSON *id_item = cJSON_GetObjectItem(root, "id");
            cJSON *x_item = cJSON_GetObjectItem(root, "x");
            cJSON *y_item = cJSON_GetObjectItem(root, "y");
            cJSON *z_item = cJSON_GetObjectItem(root, "z");
            cJSON *yaw_item = cJSON_GetObjectItem(root, "yaw");

            if (cJSON_IsNumber(id_item) && cJSON_IsNumber(x_item) && cJSON_IsNumber(y_item)) {
                int id = id_item->valueint;
                float x = x_item->valuedouble;  // ArUco 기준 차량의 x 좌표
                float y = y_item->valuedouble;  // ArUco 기준 차량의 y 좌표
                float z = z_item ? z_item->valuedouble : 0.0;
                float yaw = yaw_item ? yaw_item->valuedouble : 0.0;
                // 최신 좌표 및 yaw 저장
                latest_tvec[0] = x;
                latest_tvec[1] = y;
                latest_tvec[2] = z;
                latest_rvec_yaw = yaw;

                need_correction = true;

                printf("📥 수신 → ID:%d, X:%.2f, Y:%.2f, Z:%.2f, yaw:%.2fcm\n", id, x, y, z, yaw);
            }
        }
    }
    
    cJSON_Delete(root);
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connection_lost(void *context, char *cause)
{
    printf("[경고] MQTT 연결 끊김: %s\n", cause);
}

void *button_monitor_thread(void *arg) {
    struct gpiod_line_event event;

    while (1) {
        if (gpiod_line_event_wait(line_btn, NULL) == 1) {
            gpiod_line_event_read(line_btn, &event);
            if (event.event_type == GPIOD_LINE_EVENT_FALLING_EDGE) {
                printf("👆 버튼 눌림 (GPIO27)\n");
                resume_button_pressed = 1;
            }
        }
        usleep(100000); // debounce
    }
    return NULL;
}

int main(void) {
    signal(SIGINT, handle_sigint);

    setup();  // GPIO 초기화
    // MQTT 설정
    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, connection_lost, message_arrived, NULL);  // callback 설정

    if (MQTTClient_connect(client, &opts) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "❌ MQTT 연결 실패\n");
        return 1;
    }
    start_python_script();

    // 구독 시작
    MQTTClient_subscribe(client, CMD_B, QOS);
    MQTTClient_subscribe(client, TOPIC_B_DEST, QOS);
    MQTTClient_subscribe(client, TOPIC_B_DEST_ARRIVED, QOS);
    MQTTClient_subscribe(client, TOPIC_B_POINT, QOS);
    MQTTClient_subscribe(client, TOPIC_B_DANGER, QOS);
    MQTTClient_subscribe(client, TOPIC_B_QR, QOS);


    printf("[B차] MQTT 연결 성공, 구독 시작\n");

    pthread_t button_thread;
    pthread_create(&button_thread, NULL, button_monitor_thread, NULL);


    // ===== 메인 동작 루프 =====
    while (1) {
        MQTTClient_yield(); // MQTT 콜백 처리

        if (resume_button_pressed) {
            resume_button_pressed = 0;

            const char *payload = "done";
            MQTTClient_message pubmsg = MQTTClient_message_initializer;
            pubmsg.payload = (void *)payload;
            pubmsg.payloadlen = strlen(payload);
            pubmsg.qos = QOS;
            pubmsg.retained = 0;

            MQTTClient_deliveryToken token;
            MQTTClient_publishMessage(client, TOPIC_B_COMPLETED, &pubmsg, &token);
            MQTTClient_waitForCompletion(client, token, TIMEOUT);

            printf("📤 버튼 눌림 → TOPIC_B_COMPLETED 송신: %s\n", payload);
        }


        // 👉 긴급 복귀 처리
        if (danger_detected) {
            current_goal = 'B';
            new_goal_received = 1;
            is_emergency_return = true; // 긴급 복귀 상태 설정

            danger_detected = 0;     // 플래그 초기화
            is_waiting = 0;          // 대기 상태 해제
            move_permission = 1;     // 이동 허용

            printf("🔁 경로 중단 → 'B' 목적지로 복귀\n");
        }

        if (new_goal_received && current_goal != '\0') {
            printf("➡️  A* 경로 탐색 시작: 목적지 '%c'\n", current_goal);
            Point goal = find_point_by_char(current_goal);

            if (!astar(current_pos, goal)) {
                printf("❌ 경로 탐색 실패: %c\n", current_goal);
                new_goal_received = 0;
                continue;
            }

            path_idx = 0;
            publish_status(path, path_idx, path_len);

            while (path_idx < path_len) {
                if(danger_detected)
                {
                    printf(" 긴급 복귀 중단 발생 !\n");
                    break;
                }
                while (is_waiting || !move_permission) {
                    MQTTClient_yield();
                    usleep(200000);
                }
                // --- 2. 보정: MOVE 명령 수신 직후 1회만 ---
                if (need_correction) {
                    correct_position_from_aruco(latest_tvec, latest_rvec_yaw);
                    need_correction = false;
                }
                move_permission = 0;

                Point nxt = path[path_idx];
                int td = (nxt.r < current_pos.r ? N :
                          nxt.r > current_pos.r ? S :
                          nxt.c > current_pos.c ? E  : W);
                int diff = (td - dirB + 4) % 4;
                if (diff == 3) diff = -1;

                if (diff < 0) {
                    puts("[B] TURN_LEFT");
                    rotate_one(&dirB, -1);
                } else if (diff > 0) {
                    puts("[B] TURN_RIGHT");
                    rotate_one(&dirB, +1);
                } else {
                    puts("[B] FORWARD");
                    forward_one(&current_pos, dirB);
                    path_idx++;
                }

                publish_status(path, path_idx, path_len);
                print_grid_with_dir(current_pos, dirB);
            }

            // 경로 완주 후 도착 메시지
            if (path_idx >= path_len) {
                if (current_goal == 'B') {
                    if(is_emergency_return)
                    {
                        printf("[긴급복귀] B 지점 도착 완료\n");
                        rotate_one(&dirB, 1); // 긴급 복귀 후 방향 초기화
                        forward_one(&current_pos, dirB); // 긴급 복귀 후 전진
                        is_emergency_return=false;
                    }
                    else
                    {
                        send_arrival_message(client, previous_goal);
                    }
                } else {
                    send_arrival_message(client, current_goal);
                }
                previous_goal = current_goal;
                current_goal = '\0';
                new_goal_received = 0;
            }
        }

        usleep(100000); // 0.1초
    }
    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);

    return 0;
}