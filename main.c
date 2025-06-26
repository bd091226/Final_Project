#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <gpiod.h>
#include <MQTTClient.h>
#include "encoder_c.h"
#include "sensor_A.h"
#include <unistd.h>  // sleep, usleep
#include <sys/types.h>   // pid_t
#include <sys/wait.h>    // waitpid
#include "acar.h"
#include <ctype.h>
#include <math.h>  // fabs
#include <cjson/cJSON.h>  // 반드시 포함 필요
#define CHIP_NAME "/dev/gpiochip0"

#define ADDRESS         "tcp://broker.hivemq.com:1883"  // 공용 MQTT 브로커 예시 (변경 가능)
//#define CLIENTID        "RaspberryPi_A"

#define PYTHON_SCRIPT_PATH  "/home/pi/Final_Project/aruco_stream.py"
#define PYTHON_BIN          "python3"


// MQTT 토픽
#define TOPIC_QR      "storage/gr"     // QR 전달용 (현재 주석 처리됨)
#define TOPIC_COUNT         "storage/count"       // 버튼 누른 횟수 전송용 토픽
#define TOPIC_A_STARTPOINT  "storage/startpoint"       // 출발지점 출발 알림용 토픽 ("출발 지점으로 출발")
#define TOPIC_A_STARTPOINT_ARRIVED  "storage/startpoint_arrived"       // 출발지점 도착 알림용 토픽 ("출발지점 도착")
#define TOPIC_A_DEST        "storage/dest"   // 목적지 출발 알림용 토픽
#define TOPIC_A_DEST_ARRIVED     "storage/dest_arrived"     // 목적지 도착 알림용 토픽
#define TOPIC_A_COMPLETE_ARRIVED  "storage/A_complete_arrived"

#define QOS             0       // QoS 레벨
#define TIMEOUT         10000L  // MQTT 메시지 전송 완료 대기 타임아웃(ms)

// 최신 오류값 저장
float latest_tvec[3] = {0};
float latest_rvec_yaw = 0;
bool need_correction = false;
struct gpiod_line *line_m1 = NULL;
struct gpiod_line *line_m2 = NULL;
struct gpiod_line *line_btn = NULL;

volatile sig_atomic_t keepRunning = 1; // 시그널 처리 플래그 (1: 실행중, 0: 중지 요청)
MQTTClient client;                     // 전역 MQTTClient 핸들 (콜백 및 함수들이 공유)
int count = 1;                         // 버튼 누름 횟수
int max_count =3;                      // 최대 버튼 횟수
volatile int flag_startpoint = 0;  // 전역 변수로 선언

// LED(세 가지 색)를 제어하기 위한 GPIO
extern void handle_sigint(int sig);

//volatile sig_atomic_t keepRunning = 1;
pid_t python_pid = -1;   // 파이썬 프로세스 PID 저장

// Ctrl+C 시그널 핸들러
void intHandler(int dummy) {
    keepRunning = 0;

     // LED 모두 OFF 추가
     gpiod_line_set_value(line1, 0);  // 빨강 OFF
     gpiod_line_set_value(line2, 0);  // 하양 OFF
     gpiod_line_set_value(line3, 0);  // 초록 OFF
 
     // acar.c의 SIGINT 핸들러 호출
     handle_sigint(dummy);

    // 자식(파이썬) 프로세스가 살아있으면 종료시도
    if (python_pid > 0) {
        kill(python_pid, SIGTERM);
        // 종료될 때까지 잠깐 기다려 주는 게 안전
        waitpid(python_pid, NULL, 0);
    }
}
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

//토픽과 메시지를 통신을 한 뒤 완료와 대기 후 결과 코드를 반환하여 
// 성공인지 실패인지를 구분하는 메시지가 출력되는 함수
int publish_message(const char *topic, const char *payload) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    MQTTClient_deliveryToken token;

    pubmsg.payload     = (void*)payload;
    pubmsg.payloadlen  = (int)strlen(payload);
    pubmsg.qos         = QOS;
    pubmsg.retained    = 0;

    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to publish to %s, return code %d\n", topic, rc);
        return rc;
    }

    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to wait for completion on %s, rc=%d\n", topic, rc);
    }
    return rc;
}

//송신하는 함수

// 버튼 누름 횟수를 MQTT로 전송
void send_count() {
    char payload[50];
    sprintf(payload, "%d", count);

    if (publish_message(TOPIC_COUNT, payload) == MQTTCLIENT_SUCCESS) {
        printf("[송신] Count %d → %s\n", count, TOPIC_COUNT);
    }
}
void startpoint()
{
    char msg[100];
    snprintf(msg, sizeof(msg), "A차 출발지점 도착");
    //motor_go(chip, 80, 2.10);  // 모터를 60 속도로 3초간 작동

    if (publish_message(TOPIC_A_STARTPOINT_ARRIVED, msg) == MQTTCLIENT_SUCCESS) {
        printf("[송신] %s → %s\n", msg, TOPIC_A_STARTPOINT_ARRIVED);
    }
}


// MQTT 연결 끊김 콜백
void connlost(void *context, char *cause) {
    printf("Connection lost: %s\n", cause);
}


int start_sent = 0;
/************** */
//수신하는 함수//
/************* */
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    // 수신된 메시지를 문자열로 복사 (null-terminated)
    
    char msg[message->payloadlen + 1];
    memcpy(msg, message->payload, message->payloadlen);
    msg[message->payloadlen] = '\0';

    int id;
    float x, y, z, dist;

    printf("[수신] %s → %s\n", topicName, msg);

    if (strcmp(topicName, TOPIC_A_STARTPOINT) == 0)
    {
        flag_startpoint = 1;  // 블로킹 작업 대신 플래그만 세팅
        motor_go(chip, 60, 3.0);  // 모터를 60 속도로 3초간 작동
    }
    if(strcmp(topicName, TOPIC_A_COMPLETE_ARRIVED) == 0) {
        // 목적지 도착 메시지 수신
        //motor_go(chip, 60, 3.0);  // 모터를 60 속도로 3초간 작동
    }
    if(strcmp(topicName,TOPIC_SUB)==0)
    {
        printf("[수신] %s\n", msg);
        if (!strcmp(msg,"move")) { 
            is_waiting=0;
            has_new_goal=1;
            move_permission=1; 
            puts(">> move");
        } 
        else if (!strcmp(msg,"hold"))
        { 
            is_waiting=1; 
            move_permission=0; 
            puts(">> hold"); 
        }
    }
    if(strcmp(topicName,TOPIC_A_DEST)==0)
    {
        char dest_char = msg[0];
        if (dest_char != '\0') 
        {
            if (dest_char == last_goal_char) 
            {
                // 항상 has_new_goal을 켜도록!
                current_goal_char = dest_char;
                last_goal_char = dest_char;
                has_new_goal = 1;
                printf(">> 동일한 목적지 구역입니다: %c\n", dest_char);
            } 
            else 
            {
                current_goal_char = dest_char;
                last_goal_char = dest_char;
                has_new_goal=1;
                printf(">> 새 목적지 수신: %c\n", current_goal_char);
            }
        } 
        else 
        {
            printf(">> 알 수 없는 목적지 코드: %s\n", msg);
        }
    }
    if(strcmp(topicName, TOPIC_A_COMPLETE) == 0) 
    {
        // 집으로 복귀 명령: 목적지를 'A'로 설정
        current_goal_char = 'A';
        last_goal_char = 'A';
        has_new_goal = 1;
        move_permission = 1;       // << 이 줄 추가
        is_waiting = 0;            // << 이 줄도 있으면 더 명확
        printf(">> A 차량 복귀 명령 수신: %c\n", current_goal_char);

    }
    if (strcmp(topicName, "storage/gr") == 0) 
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
        // 동적으로 할당된 메시지와 토픽 문자열 메모리 해제
        cJSON_Delete(root);
        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);
        return 1;
    }
}


int main() 
{
    int btn_value, last_btn_value;
    int ret;

    // SIGINT(Ctrl+C) 핸들러 등록
    signal(SIGINT, intHandler);
    
    // 1) GPIO 칩 오픈
    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) 
    {
        perror("gpiod_chip_open failed");
        exit(1);
    }
    ena = gpiod_chip_get_line(chip, ENA_PIN);
    enb = gpiod_chip_get_line(chip, ENB_PIN);

    if (!chip || !ena || !enb) 
    {
        perror("GPIO init failed");
        exit(1);
    }

    gpiod_line_request_output(ena, "ENA", 1);
    gpiod_line_request_output(enb, "ENB", 1);

    // 초음파 핀 초기화
    //init_ultrasonic_pins(chip);

    // 2) 파이썬 스크립트 실행 (Flask 서버 띄우기)
    start_python_script();

    // 2) 모터 제어용 GPIO (IN1, IN2)
    line_m1 = gpiod_chip_get_line(chip, MOTOR_IN1);
    line_m2 = gpiod_chip_get_line(chip, MOTOR_IN2);

    // 버튼 GPIO
    line_btn = gpiod_chip_get_line(chip, BUTTON_PIN);
    if (!line_m1 || !line_m2 || !line_btn) {
        perror("gpiod_chip_get_line (motor/button)");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    // 3) LED 제어용 GPIO (빨강, 하양, 초록)
    line1 = gpiod_chip_get_line(chip, GPIO_LINE1);
    line2 = gpiod_chip_get_line(chip, GPIO_LINE2);
    line3 = gpiod_chip_get_line(chip, GPIO_LINE3);
    if (!line1 || !line2 || !line3) {
        perror("gpiod_chip_get_line (led)");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    // 4) 모터 제어용 GPIO (초기값 OFF)
    if (gpiod_line_request_output(line_m1, "motor_ctrl", 0) < 0 ||
        gpiod_line_request_output(line_m2, "motor_ctrl", 0) < 0) 
    {
        perror("gpiod_line_request_output (motor)");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    // 5) LED 제어용 GPIO (초기값 OFF)
    ret = gpiod_line_request_output(line1, "led_ctrl", 0);
    ret |= gpiod_line_request_output(line2, "led_ctrl", 0);
    ret |= gpiod_line_request_output(line3, "led_ctrl", 0);
    if (ret < 0) {
        perror("gpiod_line_request_output (led)");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    // 6) 버튼 입력용 GPIO
    if (gpiod_line_request_input(line_btn, "btn_read") < 0) {
        perror("gpiod_line_request_input (button)");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    // ENA/ENB 출력 설정
    gpiod_line_request_output(ena, "ENA", 1);
    gpiod_line_request_output(enb, "ENB", 1);

    // 방향 제어 핀
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

    // 7) MQTT 클라이언트 생성 및 연결
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;
    // 콜백 함수 등록: 연결 끊김, 메시지 수신
    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, NULL);

    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to MQTT broker, return code %d\n", MQTTClient_connect(client, &conn_opts));
        gpiod_chip_close(chip);
        return -1;
    }

    MQTTClient_subscribe(client, TOPIC_A_DEST, QOS);
    MQTTClient_subscribe(client, TOPIC_A_STARTPOINT, QOS);
    MQTTClient_subscribe(client, TOPIC_A_DEST_ARRIVED, QOS);
    MQTTClient_subscribe(client, TOPIC_SUB, QOS);
    MQTTClient_subscribe(client, TOPIC_A_COMPLETE, QOS);
    MQTTClient_subscribe(client, TOPIC_QR, QOS);

    printf("MQTT connected. Waiting for button press...\n");
    gpiod_line_set_value(line3, 1);  // 초록 LED ON
    

    // 9) 버튼의 마지막 상태
    last_btn_value = gpiod_line_get_value(line_btn);

    // 10) 메인 루프: keepRunning이 1인 동안 반복
    while (keepRunning) 
    {
        // 버튼 상태(LOW: 눌림, HIGH: 풀업)
        btn_value = gpiod_line_get_value(line_btn);

        // 모터 제어: 버튼을 누르고 있으면 전진 (IN1=1, IN2=0)
        if (btn_value == 0) {
            gpiod_line_set_value(line_m1, 1);
            gpiod_line_set_value(line_m2, 0);
        } else {
            // 버튼을 떼면 모터 정지 (IN1=0, IN2=0)
            gpiod_line_set_value(line_m1, 0);
            gpiod_line_set_value(line_m2, 0);
        }

        if (last_btn_value == 0 && btn_value == 1) {
            send_count();

            if(count < max_count) {
                count++;
            } else {
                count = 1;
            }
        }
        last_btn_value = btn_value;
        // MQTT 메시지 처리
        MQTTClient_yield();
        // 플래그 확인하여 블로킹 작업 수행
        if (flag_startpoint) {
            // LED 빨강 ON, 초록 OFF
            gpiod_line_set_value(line1, 1);
            gpiod_line_set_value(line3, 0);

            usleep(3000000);  // 3초 딜레이 (블로킹 문제 없도록 메인 루프 내에서)

            // LED 하양 ON, 빨강 OFF
            gpiod_line_set_value(line1, 0);
            gpiod_line_set_value(line2, 1);

            startpoint();  // 필요한 함수 호출
            start_sent = 1;

            flag_startpoint = 0;  // 처리 완료 후 플래그 초기화
        }
        if (!has_new_goal) continue;

        printf("경로 재계산 요청: 현재 위치=(%d,%d), 목적지='%c'\n",
        current_pos.r, current_pos.c, current_goal_char);
       
        Point g = find_point_by_char(current_goal_char);
        if (current_goal_char == '\0' || !isalpha(current_goal_char)) {
            printf("목적지 문자가 유효하지 않음: '%c'\n", current_goal_char);
            has_new_goal = 0;
            continue;
        }
        if (!astar(current_pos, g)) {
            printf("A* 실패: (%d,%d) → (%d,%d)\n", current_pos.r, current_pos.c, g.r, g.c);
            path_len = 0;
            path_idx = 0;
            memset(path, 0, sizeof(path));  // path[] 배열 완전 초기화
            has_new_goal = 0;
            continue;
        }

        path_idx = 0;
        publish_multi_status(path, path_idx, path_len);
        has_new_goal = 0; // 목표 수신 완료 후 초기화

        while (path_idx < path_len) 
        {
            while (is_waiting || !move_permission) 
            {
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
            int td = (nxt.r < current_pos.r ? NORTH :
                      nxt.r > current_pos.r ? SOUTH :
                      nxt.c > current_pos.c ? EAST  : WEST);
            int diff = (td - dirA + 4) % 4;
            if (diff == 3) diff = -1;

            if (diff < 0) {
                puts("[A] TURN_LEFT");
                rotate_one(&dirA, -1);  // 속도 70으로 좌회전
            } else if (diff > 0) {
                puts("[A] TURN_RIGHT");
                rotate_one(&dirA, +1);  // 속도 70으로 우회전
            } else {
                puts("[A] FORWARD");
                forward_one(&current_pos, dirA);  // 속도 70으로 전진
                path_idx++;
            }
            
            publish_multi_status(path, path_idx, path_len);
            print_grid_with_dir(current_pos, dirA);
        }
        char msg_buffer[10];
        sprintf(msg_buffer, "%c", current_goal_char);
        // 목적지에 따라 다른 토픽으로 전송
        const char *target_topic = (current_goal_char == 'A') ? TOPIC_A_COMPLETE_ARRIVED : TOPIC_A_DEST_ARRIVED;
        if (publish_message(target_topic, msg_buffer) == MQTTCLIENT_SUCCESS) 
        {
            // 컨베이어벨트 작동
            struct timespec start, now;
            double duration = 0.4;
            struct gpiod_line *line_m1, *line_m2;

            // 모터 제어용 GPIO (IN1, IN2)
            line_m1 = gpiod_chip_get_line(chip, MOTOR_IN1);
            line_m2 = gpiod_chip_get_line(chip, MOTOR_IN2);

            // 모터 ON
            gpiod_line_set_value(line_m1, 1);
            gpiod_line_set_value(line_m2, 0);

            clock_gettime(CLOCK_MONOTONIC, &start);

            while (1) {
                clock_gettime(CLOCK_MONOTONIC, &now);
                double elapsed = (now.tv_sec - start.tv_sec) + 
                                (now.tv_nsec - start.tv_nsec) / 1e9;

                if (elapsed >= duration) {
                    // 1.5초 경과하면 모터 OFF 후 종료
                    gpiod_line_set_value(line_m1, 0);
                    gpiod_line_set_value(line_m2, 0);
                    break;
                }
                usleep(10000);       // CPU 점유 최소화
            }
            printf("[송신] %s → %s\n", msg_buffer, TOPIC_A_DEST_ARRIVED);
        } 
        else {
            printf("[오류] 목적지 도착 메시지 전송 실패: %s\n", msg_buffer);
        }
        current_goal_char = '\0';
        last_goal_char = '\0';
        path_idx = 0;
        path_len = 0;
        has_new_goal = 0;
        memset(path, 0, sizeof(path));
    }

    // 모터 OFF
    gpiod_line_set_value(line_m1, 0);
    gpiod_line_set_value(line_m2, 0);

    // MQTT 연결 해제
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    // GPIO해제
    gpiod_line_release(line_m1);
    gpiod_line_release(line_m2);
    gpiod_line_release(line_btn);
    gpiod_line_release(line1);
    gpiod_line_release(line2);
    gpiod_line_release(line3);

    // GPIO 칩 닫기
    gpiod_chip_close(chip);
    printf("Program terminated.\n");
    return 0;
}