#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h>

#define ADDRESS     "tcp://broker.hivemq.com:1883"
#define CLIENTID    "RaspberryPi_Container"   // 다른 클라이언트 ID 사용 권장
#define TOPIC_COUNT       "storage/count" // count 값 수신
#define PUB_TOPIC   "storage/startdest" 
#define QOS         1
#define TIMEOUT     10000L

MQTTClient client;

volatile int connected = 0; // 연결 여부 확인

void delivered(void *context, MQTTClient_deliveryToken dt) {
    // 메시지 발송 완료 콜백 (필요시 사용)
}

// 목적지 출발 메시지 발송 함수
void send_startdest() {
    const char* msg = "목적지 출발";

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void*)msg;
    pubmsg.payloadlen = (int)strlen(msg);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, PUB_TOPIC, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("Failed to publish start command, return code %d\n", rc);
        return;
    }
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("Start command message delivered\n");
}

// 메시지가 도착 했을때 호출 되는 것
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char* payloadptr = (char*)message->payload;

    // 수신 메시지를 문자열로 복사
    char msgPayload[message->payloadlen + 1];
    memcpy(msgPayload, payloadptr, message->payloadlen);
    msgPayload[message->payloadlen] = '\0';

    printf("Received message on topic %s: %s\n", topicName, msgPayload);

    // 문자열을 정수로 변환
    int count = atoi(msgPayload);

    // count가 2 초과일 때 목적지 출발 메시지 전송
    if (count > 2) {
        send_startdest();
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return 1;
}

// 브로커와 연결 끊겼을 때 호출되는 콜백 함수
void connlost(void *context, char *cause) {
    printf("Connection lost: %s\n", cause);
    connected = 0;
}

int main(int argc, char* argv[]) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_create(&client, ADDRESS, CLIENTID,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);

    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);
    // connlost : 연결 끊김 콜백
    // msgarrvd : 메시지 수신 콜백
    // delivered : 메시지 발송 완료 콜백

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        return -1;
    }

    // 연결 성공시 출력
    printf("Connected to MQTT broker, subscribing to topic: %s\n", TOPIC_COUNT); 
    MQTTClient_subscribe(client, TOPIC_COUNT, QOS);

    // 메시지 수신을 계속 대기 (무한 루프)
    while(1) {
        // sleep 등으로 CPU 점유율 낮추기
        #ifdef _WIN32
            Sleep(1000);
        #else
            sleep(1);
        #endif
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return 0;
}
