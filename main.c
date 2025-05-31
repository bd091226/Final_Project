// void QR_read()
// {
//     FILE *fp;
//     char result[128];
//     fp = popen("python3 /home/pi/FinalProject/A_camera_final.py", "r");  // 파일 경로에 맞게 수정
//     if (fp == NULL) {
//         printf("Failed to run Python script\n");
//         return;
//     }
//     // 전체 출력 한 번에 받기
//     if (fgets(result, sizeof(result), fp) != NULL) {
//         result[strcspn(result, "\r\n")] = '\0';  // 첫 줄 개행 제거
//         char *newline = strchr(result, '\n');   // 줄바꿈 문자 찾기 (이건 fgets에서는 잘 안 씀)
//         // 줄바꿈으로 분리
//         char *zone = strtok(result, "\n");
//         char *product = strtok(NULL, "\n");
//         if (zone && product) {
//             // zone_id 발행
//             MQTTClient_message pub1 = MQTTClient_message_initializer;
//             pub1.payload = zone;
//             pub1.payloadlen = strlen(zone);
//             pub1.qos = QOS;
//             pub1.retained = 0;
//             MQTTClient_deliveryToken token1;
//             MQTTClient_publishMessage(client, TOPIC_QR, &pub1, &token1);
//             printf("[PUBLISH] zone_id: %s → %s\n", zone, TOPIC_QR);
//             // product_id 발행
//             MQTTClient_message pub2 = MQTTClient_message_initializer;
//             pub2.payload = product;
//             pub2.payloadlen = strlen(product);
//             pub2.qos = QOS;
//             pub2.retained = 0;
//             MQTTClient_deliveryToken token2;
//             MQTTClient_publishMessage(client, "storage/product", &pub2, &token2);
//             printf("[PUBLISH] product_id: %s → storage/product\n", product);
//         } else {
//             printf("Invalid QR format. Expected two lines.\n");
//         }
//     }
//     pclose(fp);
// }

// main.c

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <gpiod.h>
#include <MQTTClient.h>
#include <unistd.h>  // sleep, usleep

#define ADDRESS         "tcp://broker.hivemq.com:1883"  // 공용 MQTT 브로커 예시 (변경 가능)
#define CLIENTID        "RaspberryPi_A"

// MQTT 토픽
//#define TOPIC_QR      "storage/gr"     // QR 전달용 (현재 주석 처리됨)
#define TOPIC_COUNT       "storage/count"       // 버튼 누른 횟수 전송용 토픽
#define TOPIC_A_START     "storage/start"       // 출발 알림용 토픽 ("출발했습니다.")
#define TOPIC_A_STARTDEST "storage/startdest"   // 목적지 출발 알림용 토픽
#define TOPIC_A_ARRIVED   "storage/arrived"     // 목적지 도착 알림용 토픽
#define TOPIC_A_HOME      "storage/home"        // 집으로 복귀 알림용 토픽

#define QOS             0       // QoS 레벨
#define TIMEOUT         10000L  // MQTT 메시지 전송 완료 대기 타임아웃(ms)

// GPIO 디바이스 경로 및 핀 번호 (BCM 번호)
#define GPIO_CHIP       "/dev/gpiochip0"
#define GPIO_LINE1      26  // 빨강 LED
#define GPIO_LINE2      19  // 하양 LED
#define GPIO_LINE3      16  // 초록 LED

#define MOTOR_IN1       22  // L298N IN1
#define MOTOR_IN2       27  // L298N IN2
#define BUTTON_PIN      17  // 버튼 입력 핀

volatile sig_atomic_t keepRunning = 1; // 시그널 처리 플래그 (1: 실행중, 0: 중지 요청)
MQTTClient client;                     // 전역 MQTTClient 핸들 (콜백 및 함수들이 공유)
int count = 1;                         // 버튼 누름 횟수

// LED(세 가지 색)를 제어하기 위한 GPIO
struct gpiod_line *line1, *line2, *line3;

// keepRunning을 0으로 설정하여 메인 루프 종료
void intHandler(int dummy) {
    keepRunning = 0;
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

//======================================//
//        송신하는 함수 모음 시작         //
//======================================//

// 버튼 누름 횟수를 MQTT로 전송
void send_count() {
    char payload[50];
    sprintf(payload, "%d", count);

    if (publish_message(TOPIC_COUNT, payload) == MQTTCLIENT_SUCCESS) {
        printf("[PUBLISH] Count %d → %s\n", count, TOPIC_COUNT);
    }
}

// 목적지로 출발했음을 알리는 메시지 발행
void send_start_msg(const char* dest) {
    char msg[100];
    snprintf(msg, sizeof(msg), "%s로 출발했음", dest);

    if (publish_message(TOPIC_A_START, msg) == MQTTCLIENT_SUCCESS) {
        printf("[PUBLISH] %s → %s\n", msg, TOPIC_A_START);
    }
}

// 목적지 도착 시 실행되는 함수
// 흰색 LED를 끄고, MQTT로 "목적지 도착" 메시지 발행
void send_arrived() {
    const char* msg = "목적지 도착";
    // 흰색 LED(line2) OFF
    gpiod_line_set_value(line2, 0);

    if (publish_message(TOPIC_A_ARRIVED, msg) == MQTTCLIENT_SUCCESS) {
        printf("[PUBLISH] %s → %s\n", msg, TOPIC_A_ARRIVED);
    }
}



//======================================//
//       수신하는 함수 모음 시작         //
//======================================//

// MQTT 연결 끊김 콜백
void connlost(void *context, char *cause) {
    printf("Connection lost: %s\n", cause);
}

// 메시지 수신 콜백: storage/startdest 토픽을 처리
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    // 수신된 메시지를 문자열로 복사 (null-terminated)
    char msg[message->payloadlen + 1];
    memcpy(msg, message->payload, message->payloadlen);
    msg[message->payloadlen] = '\0';

    printf("[RECV] %s → %s\n", topicName, msg);

    // 목적지 출발 알림을 위한 처리
    if (strcmp(topicName, TOPIC_A_STARTDEST) == 0) {
        // 빨강 LED(line1) ON → OFF
        gpiod_line_set_value(line1, 1);
        sleep(2);
        gpiod_line_set_value(line1, 0);
        sleep(1);

        // 하얀 LED(line2) ON (출발 준비 시그널)
        gpiod_line_set_value(line2, 1);
        sleep(5);
        // 하얀 LED OFF
        gpiod_line_set_value(line2, 0);

        // "dest로 출발했음" 메시지 발행
        send_start_msg(msg);

        // 목적지 도착 메시지 발행
        send_arrived();

        // 버튼 카운트 초기화
        count = 1;
    }

    // 동적으로 할당된 메시지와 토픽 문자열 메모리 해제
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}


int main() {
    struct gpiod_chip *chip;
    struct gpiod_line *line_m1, *line_m2, *line_btn;
    int btn_value, last_btn_value;
    int ret;

    // SIGINT(Ctrl+C) 핸들러 등록
    signal(SIGINT, intHandler);

    // 1) GPIO 칩 오픈
    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("gpiod_chip_open");
        return EXIT_FAILURE;
    }

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
        gpiod_line_request_output(line_m2, "motor_ctrl", 0) < 0) {
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

    // 7) MQTT 클라이언트 생성 및 연결
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    // 콜백 함수 등록: 연결 끊김, 메시지 수신
    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, NULL);

    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to MQTT broker, return code %d\n", MQTTClient_connect(client, &conn_opts));
        gpiod_chip_close(chip);
        return -1;
    }

    printf("MQTT connected. Waiting for button press...\n");

    // 8) 목적지 출발 토픽 구독
    MQTTClient_subscribe(client, TOPIC_A_STARTDEST, QOS);

    // 9) 버튼의 마지막 상태
    last_btn_value = gpiod_line_get_value(line_btn);

    // 10) 메인 루프: keepRunning이 1인 동안 반복
    while (keepRunning) {
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

        // 눌림 엣지 감지: last_btn_value가 1이고 현재 btn_value가 0인 경우
        if (last_btn_value == 1 && btn_value == 0) {
            send_count();
            count++;
        }
        last_btn_value = btn_value;

        // 0.1초(100ms) 대기: 디바운싱 처리
        usleep(100000);
    }

    //======================================//
    //           종료 시 정리 작업           //
    //======================================//

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

    return 0;
}
