#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <MQTTClient.h>

#define ADDRESS     "tcp://broker.hivemq.com:1883"  // 공용 MQTT 브로커 예시 (변경 가능)
#define CLIENTID    "RaspberryPi_A"
#define TOPIC_COUNT       "storage/count"
#define TOPIC_A_STARTDEST      "storage/startdest"  // 목적지 출발 알림
#define TOPIC_A_ARRIVED   "storage/arrived" // 목적지 도착 알림
#define QOS         1
#define TIMEOUT     10000L

#define BUTTON_PIN  0  // wiringPi 핀번호 (예: GPIO17)

MQTTClient client;
int count = 1;


// “목적지 도착” 메시지 발송 함수
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
    MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("[PUBLISH] %s → %s\n", msg, TOPIC_A_ARRIVED);
}

// 연결 끊김 콜백
void connlost(void *context, char *cause) {
    printf("Connection lost: %s\n", cause);
}
void send_count() {
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

// 메시지 수신 콜백
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char msg[message->payloadlen + 1];
    memcpy(msg, message->payload, message->payloadlen);
    msg[message->payloadlen] = '\0';

    // 목적지 출발 메시지 처리
    if (strcmp(topicName, TOPIC_A_STARTDEST) == 0) {
        printf("[COMMAND RECEIVED] %s\n", msg);
        count = 1;

        delay(3000); // 1초 대기 후 목적지 도착 메시지 발송
        send_arrived();
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

int main() {
    int rc;

    wiringPiSetup();
    pinMode(BUTTON_PIN, INPUT);
    pullUpDnControl(BUTTON_PIN, PUD_UP);  // 풀업저항 설정

    MQTTClient_create(&client, ADDRESS, CLIENTID,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, NULL);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to MQTT broker, return code %d\n", rc);
        return -1;
    }

    printf("MQTT connected. Waiting for button press...\n");
    MQTTClient_subscribe(client, TOPIC_A_STARTDEST, QOS);

    int lastButtonState = HIGH;
    while (1) {
        int buttonState = digitalRead(BUTTON_PIN);
        if (lastButtonState == HIGH && buttonState == LOW) {
            // 버튼 눌림 감지
            
            printf("Button pressed! Sending count: %d\n", count);
            send_count();
            count++;
        }
        lastButtonState = buttonState;
        delay(100); // 100ms 딜레이 (디바운싱)
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return 0;
}
