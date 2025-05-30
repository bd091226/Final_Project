//main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <gpiod.h>
#include <MQTTClient.h>
#include <unistd.h>  // sleep, usleep

#define ADDRESS     "tcp://broker.hivemq.com:1883"  // 공용 MQTT 브로커 예시 (변경 가능)
#define CLIENTID    "RaspberryPi_A"
//#define TOPIC_QR    "storage/gr"// QR 전달용
#define TOPIC_COUNT       "storage/count"
#define TOPIC_A_START      "storage/start"      // 출발 알림용 새 토픽 // "출발했습니다."
#define TOPIC_A_STARTDEST      "storage/startdest"  // 목적지 출발 알림 
#define TOPIC_A_ARRIVED   "storage/arrived" // 목적지 도착 알림
#define QOS         0
#define TIMEOUT     10000L

#define GPIO_CHIP   "/dev/gpiochip0"
#define GPIO_LINE1  26 // 빨강
#define GPIO_LINE2  6 // 하양
#define GPIO_LINE3  16 // 초록

#define MOTOR_IN1 22   // L298N IN1
#define MOTOR_IN2 27   // L298N IN2
#define BUTTON_PIN 17  // 버튼 입력

volatile sig_atomic_t keepRunning = 1;
// SIGINT(Ctrl+C) 핸들러
MQTTClient client;
int count = 1; //버튼을 누른 횟수
struct gpiod_line  *line1, *line2, *line3;

void intHandler(int dummy) {
    keepRunning = 0;
}

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


void send_arrived() {
    const char* msg = "목적지 도착";

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void*)msg;
    pubmsg.payloadlen = (int)strlen(msg);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, TOPIC_A_ARRIVED, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("Failed to publish arrived message, rc=%d\n", rc);
        return;
    }

    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("Failed to wait for completion, rc=%d\n", rc);
        return;
    }

    printf("[PUBLISH] %s → %s\n", msg, TOPIC_A_ARRIVED);
}


// 연결 끊김 콜백
void connlost(void *context, char *cause) {
    printf("Connection lost: %s\n", cause);
}
void send_count() { // 버튼을 누른 횟수를 MQTT로 전송
    char payload[50];
    sprintf(payload, "%d", count);

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, TOPIC_COUNT, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("Failed to publish message, return code %d\n", rc);
        return;
    }
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("Message with count %d delivered\n", count);
}

// 메시지 수신 
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    //수신된 메세지를 문자열로 복사
    char msg[message->payloadlen + 1];
    memcpy(msg, message->payload, message->payloadlen);
    msg[message->payloadlen] = '\0';

    // 구독한 토픽이 'storage/startdest'일 때만 처리
    if (strcmp(topicName, TOPIC_A_STARTDEST) == 0) { // 목적지를 수신받음
        // 출발 메시지 전송(출발한다)
        gpiod_line_set_value(line1, 1);
        sleep(2);
        char startMsg[100];
        snprintf(startMsg, sizeof(startMsg), "%s로 출발했음", msg);

        MQTTClient_message pubmsg = MQTTClient_message_initializer;
        pubmsg.payload = startMsg;
        pubmsg.payloadlen = (int)strlen(startMsg);
        pubmsg.qos = QOS;
        pubmsg.retained = 0;

        MQTTClient_deliveryToken token;

        //출발했다는 메시지를 발행
        int rc = MQTTClient_publishMessage(client,TOPIC_A_START,&pubmsg,&token);
        if (rc != MQTTCLIENT_SUCCESS) {
            printf("Failed to publish start message, rc=%d\n", rc);
        } else {
            // 출발 메시지가 성공적으로 발행되었을 때
            // 메시지 전송 완료까지 대기 (블로킹)
            MQTTClient_waitForCompletion(client, token, TIMEOUT);

            printf("[PUBLISH] %s → %s\n", startMsg, TOPIC_A_START);
            count = 1;
            // 목적지 도착 메시지 발송 전송
            send_arrived();
        }
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}


int main() {
    struct gpiod_chip  *chip;
    struct gpiod_servo  *line_m1, *line_m2, *line_btn;
    int rc,btn_value, last_btn_value;
    int ret;

    signal(SIGINT, intHandler);

    // 1) GPIO 칩 오픈
    chip = gpiod_chip_open("/dev/gpiochip0");
    if (!chip) {
        perror("gpiod_chip_open");
        return EXIT_FAILURE;
    }

    // servo 라인 가져오기
    line_m1 = gpiod_chip_get_line(chip, MOTOR_IN1);
    line_m2 = gpiod_chip_get_line(chip, MOTOR_IN2);
    line_btn = gpiod_chip_get_line(chip, BUTTON_PIN);
    if (!line_m1 || !line_m2 || !line_btn) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    // LED 관련 GPIO 라인 가져오기
    line1 = gpiod_chip_get_line(chip, GPIO_LINE1);
    line2 = gpiod_chip_get_line(chip, GPIO_LINE2);
    line3 = gpiod_chip_get_line(chip, GPIO_LINE3);
    if (!line1 || !line2 || !line3) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        exit(EXIT_FAILURE);
    }

    // 3) 출력용 요청 (초기값 LOW)
    if (gpiod_line_request_output(line_m1, "led_ctrl", 0) < 0 ||
        gpiod_line_request_output(line_m2, "led_ctrl", 0) < 0) {
        perror("gpiod_line_request_output (motor)");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    // 3) 각 라인을 출력(output) 모드로 요청 (초기 값은 0 = OFF)
    ret = gpiod_line_request_output(line1, "led_blink", 0);
    ret |= gpiod_line_request_output(line2, "led_blink", 0);
    ret |= gpiod_line_request_output(line3, "led_blink", 0);
    if (ret < 0) {
        perror("gpiod_line_request_output");
        gpiod_chip_close(chip);
        exit(EXIT_FAILURE);
    }

    // 4) 입력용 요청 (풀업은 하드웨어/디바이스 트리에서)
    if (gpiod_line_request_input(line_btn, "btn_read") < 0) {
        perror("gpiod_line_request_input (button)");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    // MQTT 클라이언트 생성·연결
    MQTTClient_create(&client, ADDRESS, CLIENTID,MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, NULL);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to MQTT broker, return code %d\n", rc);
        return -1;
    }

    printf("MQTT connected. Waiting for button press...\n");

    QR_read();
    // 목적지 출발 토픽 구독
    MQTTClient_subscribe(client, TOPIC_A_STARTDEST, QOS);

     last_btn_value = gpiod_line_get_value(line_btn);
    

    while (keepRunning) {
        // 버튼 상태 읽기 (LOW: 눌림, HIGH: 풀업)
        btn_value = gpiod_line_get_value(line_btn);

        // 모터 제어: 버튼 누르고 있으면 전진
        if (btn_value == 0) {
            gpiod_line_set_value(line_m1, 1);
            gpiod_line_set_value(line_m2, 0);
        } else {
            gpiod_line_set_value(line_m1, 0);
            gpiod_line_set_value(line_m2, 0);
        }

        // 눌림 엣지 감지 시 count 전송
        if (last_btn_value == 1 && btn_value == 0) {
            send_count();
            count++;
        }
        last_btn_value = btn_value;

        usleep(100000);  // 100ms 디바운싱
    }


    // 종료 시 정리
    gpiod_line_set_value(line_m1, 0);
    gpiod_line_set_value(line_m2, 0);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    gpiod_line_release(line_m1);
    gpiod_line_release(line_m2);
    gpiod_line_release(line_btn);
    gpiod_chip_close(chip);

    return 0;
}
