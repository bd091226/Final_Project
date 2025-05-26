#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h> // MQTTClient.h는 MQTT 클라이언트 라이브러리의 헤더 파일
#include <unistd.h>

#define ADDRESS         "tcp://broker.hivemq.com:1883"
#define CLIENTID        "Bcar_Container"  // 다른 클라이언트 ID 사용 권장
#define TOPIC_B_START   "storage/b_startdest"
#define TOPIC_B_ARRIVED "storage/b_arrived"
#define QOS             1
#define TIMEOUT         10000L


MQTTClient client; 

// db_access.py에서 목적지 구역 ID를 가져오는 함수
char* fetch_saturated_zone() {
    static char result[64];
    FILE *fp = popen("python3 -c \""
    "from db_access import fetch_saturated_zone; "
    "zone = fetch_saturated_zone(); "
    "print(zone if zone else '')"
    "\"", 
    "r");
    // popen : 쉘에서 파이썬 인터프리터를 실행
    
    if (fp == NULL) {
        fprintf(stderr, "Failed to run Python inline script\n");
        return NULL;
    }
    if (fgets(result, sizeof(result), fp) != NULL) {
        result[strcspn(result, "\n")] = 0;  // 개행 제거
    } else {
        pclose(fp);
        return NULL;
    }
    pclose(fp);
    return result;
}

// 메시지 수신 함수
int message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message) {
    char msg[message->payloadlen + 1];
    memcpy(msg, message->payload, message->payloadlen);
    msg[message->payloadlen] = '\0';

    printf("[수신] 토픽: %s, 메시지: %s\n", topicName, msg);

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}

// 메시지 송신 함수
void publish_zone(const char *zone_id) {
    MQTTClient_message pubmsg = MQTTClient_message_initializer; // 메세지 구조체 초기화
    char payload[64];

    //페이로드 버퍼에 목적지 구역 ID를 복사
    snprintf(payload, sizeof(payload), "%s", zone_id); 
    //snprintf : 문자열을 형식(format)에 맞게 만들어서 문자열 변수에 저장할 수 있도록 해주는 함수
    
    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    // MQTTClient_publishMessage : MQTT 메시지를 발행하는 함수
    int rc = MQTTClient_publishMessage(client, TOPIC_B_START, &pubmsg, &token);
    
    if (rc == MQTTCLIENT_SUCCESS) {
        // MQTTClient_waitForCompletion(client, token, TIMEOUT);
        printf("[발행] %s → %s\n", TOPIC_B_START, zone_id);
    } else {
        fprintf(stderr, "MQTT publish failed, rc=%d\n", rc);
    }
}

// 콜백: 연결 끊겼을 때 호출됩니다
void connection_lost(void *context, char *cause) {
    fprintf(stderr, "[경고] MQTT 연결 끊김: %s\n", cause);
}
int main(int argc, char *argv[]) {
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    //전역 clinet 객체를 생성
    MQTTClient_create(&client, ADDRESS, CLIENTID,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, connection_lost,
                        message_arrived, NULL);
    //브로커에 연결
    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "Failed to connect to MQTT broker\n");
        return -1;
    }
    printf("Connected to MQTT broker at %s\n", ADDRESS);

    MQTTClient_subscribe(client, TOPIC_B_ARRIVED, QOS);
    printf("Subscribed to topic: %s\n", TOPIC_B_ARRIVED);
    // 한 번만 구역 ID 조회 & 발행
    char *zone = fetch_saturated_zone();
    if (zone && *zone) {
        publish_zone(zone);
    } else {
        printf("조회된 구역이 없습니다.\n");
    }

    // 주기적 또는 이벤트 기반 호출 예시
    while (1) {
        sleep(1); // 5초 대기
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    return 0;
}