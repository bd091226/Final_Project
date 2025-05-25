#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h>
#include <unistd.h>

#define ADDRESS "tcp://broker.hivemq.com:1883"
#define CLIENTID "RaspberryPi_Container" // 다른 클라이언트 ID 사용 권장
#define TOPIC "storage/count"
#define QOS 1
#define TIMEOUT 10000L

MQTTClient client;

volatile int connected = 0;

void delivered(void *context, MQTTClient_deliveryToken dt)
{
    // 메시지 발송 완료 콜백 (필요시 사용)
}

int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    char *payloadptr = (char *)message->payload;

    // 수신 메시지를 문자열로 복사
    char msgPayload[message->payloadlen + 1];
    memcpy(msgPayload, payloadptr, message->payloadlen);
    msgPayload[message->payloadlen] = '\0';

    printf("Received message on topic %s: %s\n", topicName, msgPayload);

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return 1;
}

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

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        return -1;
    }

    printf("Connected to MQTT broker, subscribing to topic: %s\n", TOPIC);
    MQTTClient_subscribe(client, TOPIC, QOS);

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
