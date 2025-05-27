#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <MQTTClient.h>
#include <unistd.h>

#define ADDRESS     "tcp://broker.hivemq.com:1883"  // 공용 MQTT 브로커 예시 (변경 가능)
#define CLIENTID    "RaspberryPi_A"
#define TOPIC_COUNT       "storage/count"
#define TOPIC_A_START      "storage/start"      // 출발 알림용 새 토픽 // "출발했습니다."
#define TOPIC_A_STARTDEST      "storage/startdest"  // 목적지 출발 알림 
#define TOPIC_A_ARRIVED   "storage/arrived" // 목적지 도착 알림
#define QOS         0
#define TIMEOUT     10000L

#define BUTTON_PIN  0  // wiringPi 핀번호 (예: GPIO17)

MQTTClient client;
int count = 1; //버튼을 누른 횟수


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

    wiringPiSetup();
    pinMode(BUTTON_PIN, INPUT);
    pullUpDnControl(BUTTON_PIN, PUD_UP);  // 풀업저항 설정

    // MQTT 클라이언트 생성·연결
    MQTTClient_create(&client, ADDRESS, CLIENTID,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, NULL);

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to MQTT broker, return code %d\n", rc);
        return -1;
    }

    printf("MQTT connected. Waiting for button press...\n");
    // 목적지 출발 토픽 구독
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
