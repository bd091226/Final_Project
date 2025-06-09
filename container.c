#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "MQTTClient.h"

#define ADDRESS "tcp://localhost:1883"
#define CLIENTID "Storage"
#define TOPIC_SUB "vehicle/status"
#define TOPIC_PUB "vehicle/storage/A"
#define QOS 0
#define TIMEOUT 10000L

MQTTClient client;

void send_message_to_vehicle_A(const char *msg)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;

    pubmsg.payload = (void *)msg;
    pubmsg.payloadlen = (int)strlen(msg);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, TOPIC_PUB, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "차량 A에게 메시지 송신 실패: %d\n", rc);
    }
    else
    {
        // MQTTClient_waitForCompletion(client, token, TIMEOUT);
        printf("차량 A에게 메시지 송신 완료: %s\n", msg);
    }
    MQTTClient_yield();
}
// 메시지를 수신할 때 호출되는 콜백 함수
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    char *payload = (char *)message->payload;

    // 버퍼 처리
    char buffer[message->payloadlen + 1];
    memcpy(buffer, payload, message->payloadlen);
    buffer[message->payloadlen] = '\0';

    // 수신한 메시지 출력
    printf("차량으로부터 수신한 좌표 메시지: %s\n", buffer);

    // JSON 파싱을 원한다면 여기서 파싱 가능 (예: r, c 좌표 추출)
    send_message_to_vehicle_A("move");

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

int main()
{
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

    // MQTT 클라이언트 생성
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    // 콜백 함수 등록
    MQTTClient_setCallbacks(client, NULL, NULL, msgarrvd, NULL);

    // 브로커 연결
    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "MQTT 브로커 연결 실패\n");
        exit(EXIT_FAILURE);
    }

    // vehicle/status 토픽을 구독
    MQTTClient_subscribe(client, TOPIC_SUB, QOS);

    printf("보관함 MQTT 수신 시작 (토픽: %s)...\n", TOPIC_SUB);

    // 수신 대기 루프
    while (1)
    {
        MQTTClient_yield(); // 비동기 수신 처리
        usleep(100 * 1000); // 100ms sleep
    }

    // 연결 종료 및 정리 (사실상 도달하지 않음)
    MQTTClient_disconnect(client, TIMEOUT);
    MQTTClient_destroy(&client);

    return 0;
}
