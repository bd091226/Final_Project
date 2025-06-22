#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <MQTTClient.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <pthread.h>
#include "moter_control.h"
#include "Bcar_moter.h"

#define ADDRESS "tcp://broker.hivemq.com:1883"
// #define CLIENTID "RaspberryPi_Bcar"
#define TOPIC_B_DANGER       "vehicle/emergency/B"
#define TOPIC_B_DEST "storage/b_dest"
#define TOPIC_B_DEST_ARRIVED "storage/b_dest_arrived"
#define TOPIC_B_POINT_ARRIVED "storage/b_point_arrived"
#define TOPIC_B_POINT        "storage/b_point"
#define TOPIC_B_COMPLETED "vehicle/B_completed"

volatile int danger_detected = 0; // 긴급 호출 감지 플래그
volatile int resume_button_pressed = 0;
bool is_emergency_return = false;


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
        forward_one(&current_pos, dirB, 40);
        rotate_one(&dirB, -1, 40); // 오른쪽으로 회전
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
    
    

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

void connection_lost(void *context, char *cause)
{
    printf("[경고] MQTT 연결 끊김: %s\n", cause);
}

void handle_sigint(int sig) {
    printf("\n🛑 SIGINT 감지, 프로그램 종료 중...\n");
    cleanup();  // 리소스 해제 함수
    exit(0);
}
void *button_monitor_thread(void *arg) {
    struct gpiod_line_event event;

    while (1) {
        if (gpiod_line_event_wait(button_line, NULL) == 1) {
            gpiod_line_event_read(button_line, &event);
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

    // GPIO 초기화
    //chip = gpiod_chip_open_by_name(CHIP);
    in1_line = gpiod_chip_get_line(chip, IN1_PIN);
    in2_line = gpiod_chip_get_line(chip, IN2_PIN);
    ena_line = gpiod_chip_get_line(chip, ENA_PIN);
    in3_line = gpiod_chip_get_line(chip, IN3_PIN);
    in4_line = gpiod_chip_get_line(chip, IN4_PIN);
    enb_line = gpiod_chip_get_line(chip, ENB_PIN);
    button_line = gpiod_chip_get_line(chip, BUTTON_PIN);

    gpiod_line_request_output(in1_line, "IN1", 0);
    gpiod_line_request_output(in2_line, "IN2", 0);
    gpiod_line_request_output(ena_line, "ENA", 0);
    gpiod_line_request_output(in3_line, "IN3", 0);
    gpiod_line_request_output(in4_line, "IN4", 0);
    gpiod_line_request_output(enb_line, "ENB", 0);
    gpiod_line_request_falling_edge_events(button_line, "BUTTON");

    // MQTT 설정
    MQTTClient_connectOptions opts = MQTTClient_connectOptions_initializer;
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, connection_lost, message_arrived, NULL);  // callback 설정

    if (MQTTClient_connect(client, &opts) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "❌ MQTT 연결 실패\n");
        return 1;
    }

    // 구독 시작
    MQTTClient_subscribe(client, CMD_B, QOS);
    MQTTClient_subscribe(client, TOPIC_B_DEST, QOS);
    MQTTClient_subscribe(client, TOPIC_B_DEST, QOS);
    MQTTClient_subscribe(client, TOPIC_B_POINT, QOS);
    MQTTClient_subscribe(client, TOPIC_B_DANGER, QOS);

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
                move_permission = 0;

                Point nxt = path[path_idx];
                int td = (nxt.r < current_pos.r ? N :
                          nxt.r > current_pos.r ? S :
                          nxt.c > current_pos.c ? E  : W);
                int diff = (td - dirB + 4) % 4;
                if (diff == 3) diff = -1;

                if (diff < 0) {
                    puts("[B] TURN_LEFT");
                    rotate_one(&dirB, -1, 60);
                } else if (diff > 0) {
                    puts("[B] TURN_RIGHT");
                    rotate_one(&dirB, +1, 60);
                } else {
                    puts("[B] FORWARD");
                    forward_one(&current_pos, dirB, 60);
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
                        rotate_one(&dirB, 1, 40); // 긴급 복귀 후 방향 초기화
                        forward_one(&current_pos, dirB, 40); // 긴급 복귀 후 전진
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
