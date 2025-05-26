#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h>
#include <unistd.h>

#define ADDRESS         "tcp://broker.hivemq.com:1883"
#define CLIENTID        "Bcar_Container"  // 다른 클라이언트 ID 사용 권장
#define TOPIC_B_START   "storage/b_startdest"
#define QOS             1
#define TIMEOUT         10000L

extern char* fetch_saturated_zone();  // Python 모듈로부터 zone ID를 호출

MQTTClient client;

// 메시지 발행 함수
void publish_zone(const char *zone_id) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    char payload[64];
    snprintf(payload, sizeof(payload), "출발:%s", zone_id);
    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, TOPIC_B_START, &pubmsg, &token);
    if (rc == MQTTCLIENT_SUCCESS) {
        MQTTClient_waitForCompletion(client, token, TIMEOUT);
        printf("[PUBLISH] %s → %s\n", TOPIC_B_START, payload);
    } else {
        fprintf(stderr, "MQTT publish failed, rc=%d\n", rc);
    }
}

int main(int argc, char *argv[]) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    MQTTClient_create(&client, ADDRESS, CLIENTID,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);

    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to connect to MQTT broker\n");
        return -1;
    }
    printf("Connected to MQTT broker at %s\n", ADDRESS);

    // 주기적 또는 이벤트 기반 호출 예시
    while (1) {
        char *zone = fetch_saturated_zone();
        if (zone) {
            publish_zone(zone);
        } else {
            printf("No saturated zone found.\n");
        }
        sleep(5);  // 조회 주기
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return 0;
}