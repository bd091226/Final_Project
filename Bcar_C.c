#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h>
#include <unistd.h>

#define ADDRESS         "tcp://broker.hivemq.com:1883"
#define CLIENTID        "RaspberryPi_Bcar"
#define TOPIC_B_START   "storage/b_startdest"
#define TOPIC_B_ARRIVED "storage/b_arrived"
#define QOS             1
#define TIMEOUT         10000L

MQTTClient client;

// 메시지 송신
void send_arrival(const char *zone_id) {
    // 콘솔 출력
    printf("🚗 B차, %s 구역에 도착했습니다!\n", zone_id);

    // MQTT 발행
    char payload[64];
    //페이로드 버퍼에 목적지 구역 ID를 복사
    snprintf(payload, sizeof(payload), "%s", zone_id);

    //메시지 초기화
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload    = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos        = QOS;
    pubmsg.retained   = 0;

    MQTTClient_deliveryToken token;

    int rc = MQTTClient_publishMessage(client,TOPIC_B_ARRIVED,&pubmsg,&token);
    if (rc != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "도착 메시지 발행 실패, rc=%d\n", rc);
        return;
    }
    // MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("[PUBLISH] %s → %s\n", payload, TOPIC_B_ARRIVED);
}

// 메시지 수신
int message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char msg[message->payloadlen + 1];
    memcpy(msg, message->payload, message->payloadlen);
    msg[message->payloadlen] = '\0';

    printf("[수신] 토픽: %s, 구역 ID: %s\n", topicName, msg);
    printf("🚗 B차, %s 구역으로 출발합니다!\n", msg);

    sleep(2);  // 출발 후 도착까지 딜레이
    send_arrival(msg);

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}


void connection_lost(void *context, char *cause) {
    printf("[경고] MQTT 연결 끊김: %s\n", cause);
}

int main() {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, connection_lost, message_arrived, NULL);

    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "MQTT 브로커 접속 실패\n");
        return -1;
    }

    printf("[B차] MQTT 브로커 연결 성공, 구독 시작: %s\n", TOPIC_B_START);
    MQTTClient_subscribe(client, TOPIC_B_START, QOS);

    while (1) {
        sleep(1);
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return 0;
}