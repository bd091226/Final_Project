//main.c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <wiringPi.h>
#include <MQTTClient.h>
#include <unistd.h>

#define ADDRESS     "tcp://broker.hivemq.com:1883"  // 공용 MQTT 브로커 예시 (변경 가능)
#define CLIENTID    "RaspberryPi_A"
#define TOPIC_QR    "storage/gr"// QR 전달용
#define TOPIC_COUNT       "storage/count"
#define TOPIC_A_START      "storage/start"      // 출발 알림용 새 토픽 // "출발했습니다."
#define TOPIC_A_STARTDEST      "storage/startdest"  // 목적지 출발 알림 
#define TOPIC_A_ARRIVED   "storage/arrived" // 목적지 도착 알림
#define QOS         0
#define TIMEOUT     10000L

#define MOTOR_IN1 22   // L298N IN1
#define MOTOR_IN2 27   // L298N IN2
#define BUTTON_PIN 17  // 버튼 입력

volatile sig_atomic_t keepRunning = 1;
// SIGINT(Ctrl+C) 핸들러

MQTTClient client;
int count = 1; //버튼을 누른 횟수

void intHandler(int dummy) {
    keepRunning = 0;
}

void QR_read()
{
    FILE *fp;
    char result[128];
    fp = popen("python3 /home/pi/Final_Project/A_camera_final.py", "r");  // 파일 경로에 맞게 수정

    if (fp == NULL) {
        printf("Failed to run Python script\n");
        return;
    }

    // 전체 출력 한 번에 받기
    if (fgets(result, sizeof(result), fp) != NULL) {
        result[strcspn(result, "\r\n")] = '\0';  // 첫 줄 개행 제거
        char *newline = strchr(result, '\n');   // 줄바꿈 문자 찾기 (이건 fgets에서는 잘 안 씀)

        // 줄바꿈으로 분리
        char *zone = strtok(result, "\n");
        char *product = strtok(NULL, "\n");

        if (zone && product) {
            // zone_id 발행
            MQTTClient_message pub1 = MQTTClient_message_initializer;
            pub1.payload = zone;
            pub1.payloadlen = strlen(zone);
            pub1.qos = QOS;
            pub1.retained = 0;
            MQTTClient_deliveryToken token1;
            MQTTClient_publishMessage(client, TOPIC_QR, &pub1, &token1);
            printf("[PUBLISH] zone_id: %s → %s\n", zone, TOPIC_QR);

            // product_id 발행
            MQTTClient_message pub2 = MQTTClient_message_initializer;
            pub2.payload = product;
            pub2.payloadlen = strlen(product);
            pub2.qos = QOS;
            pub2.retained = 0;
            MQTTClient_deliveryToken token2;
            MQTTClient_publishMessage(client, "storage/product", &pub2, &token2);
            printf("[PUBLISH] product_id: %s → storage/product\n", product);
        } else {
            printf("Invalid QR format. Expected two lines.\n");
        }
    }

    pclose(fp);
}


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
    int rc;

    signal(SIGINT, intHandler);
    wiringPiSetupGpio();

    pinMode(MOTOR_IN1, OUTPUT);
    pinMode(MOTOR_IN2, OUTPUT);
    pinMode(BUTTON_PIN, INPUT);
    pullUpDnControl(BUTTON_PIN, PUD_UP);  // 풀업저항 설정

    // 모터 정지 상태로 시작
    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);

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

    int lastButtonState = HIGH;
    

    while (1) {
        int buttonState = digitalRead(BUTTON_PIN);
        //printf("Button state: %d\n", buttonState);  // HIGH:1, LOW:0 출력
        

        // 모터 제어: 누르고 있으면 전진, 아니면 정지
        if (buttonState == LOW) {
            digitalWrite(MOTOR_IN1, HIGH);
            digitalWrite(MOTOR_IN2, LOW);
        } else {
            digitalWrite(MOTOR_IN1, LOW);
            digitalWrite(MOTOR_IN2, LOW);
        }

        // 눌림 엣지 감지 시 count 발송
        if (lastButtonState == HIGH && buttonState == LOW) {
            send_count();
            count++;
        }
        lastButtonState = buttonState;

        delay(100);  // 100ms 디바운싱
    }


    digitalWrite(MOTOR_IN1, LOW);
    digitalWrite(MOTOR_IN2, LOW);
    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return 0;
}
