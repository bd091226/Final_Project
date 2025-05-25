#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h>
#include <unistd.h>

#define ADDRESS     "tcp://broker.hivemq.com:1883"
#define CLIENTID    "RaspberryPi_Container"   // 다른 클라이언트 ID 사용 권장
#define TOPIC_COUNT       "storage/count" // count 값 수신
#define TOPIC_A_STARTDEST   "storage/startdest" 
#define TOPIC_A_ARRIVED "storage/arrived"  // 목적지 도착 메시지 수신 토픽
#define QOS         1
#define TIMEOUT     10000L

MQTTClient client;

volatile int connected = 0; // 연결 여부 확인

void delivered(void *context, MQTTClient_deliveryToken dt)
{
    // 메시지 발송 완료 콜백 (필요시 사용)
}

// 목적지 출발 메시지 발송 함수
void send_startdest()
{
    const char *msg = "목적지 출발";

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = (void *)msg;
    pubmsg.payloadlen = (int)strlen(msg);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, TOPIC_A_STARTDEST, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("Failed to publish start command, return code %d\n", rc);
        return;
    }
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("[PUBLISH] \"%s\\n",msg);
}

// 메시지가 도착 했을때 호출 되는 것
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    char *payloadptr = (char *)message->payload;

    // 수신 메시지를 문자열로 복사
    char msgPayload[message->payloadlen + 1];
    memcpy(msgPayload, payloadptr, message->payloadlen);
    msgPayload[message->payloadlen] = '\0';

    printf("Received message on topic %s: %s\n", topicName, msgPayload);

    // 수신한 토픽이 storage/count일 경우
    if (strcmp(topicName, TOPIC_COUNT) == 0) {
        int count = atoi(msgPayload);

        if (count > 2) {
            send_startdest();
        }
    }
    // 수신한 토픽이 storage/arrived일 경우
    if (strcmp(topicName, TOPIC_A_ARRIVED) == 0) {
        if (strcmp(msgPayload, "목적지 도착") == 0) {
            printf("✅ A차가 목적지에 도착했습니다. 필요한 동작을 수행하세요.\n");
            // 여기서 알림, 로직 처리 등 원하는 동작 수행
        }
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return 1;
}

// 브로커와 연결 끊겼을 때 호출되는 콜백 함수
void connlost(void *context, char *cause)
{
    printf("Connection lost: %s\n", cause);
    connected = 0;
}

int main(int argc, char *argv[])
{
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    MQTTClient_create(&client, ADDRESS, CLIENTID,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);

    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);
    // connlost : 연결 끊김 콜백
    // msgarrvd : 메시지 수신 콜백
    // delivered : 메시지 발송 완료 콜백

    // 이 라인 추가
    MQTTClient_subscribe(client, TOPIC_A_ARRIVED, QOS);


    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect, return code %d\n", rc);
        return -1;
    }

    // 연결 성공시 출력
    printf("Connected to MQTT broker, subscribing to topic: %s\n", TOPIC_COUNT);
    MQTTClient_subscribe(client, TOPIC_COUNT, QOS);

    // 메시지 수신을 계속 대기 (무한 루프)
    // 메시지 수신을 계속 대기 (무한 루프)
    while (1)
    {
        sleep(1); // Linux에서는 이것만 필요
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return 0;
}
