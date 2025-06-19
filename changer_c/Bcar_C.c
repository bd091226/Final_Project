/*
Bcar_C.c
컴파일 : 
gcc -o Bcar_C Bcar_C.c sensor.c -lpaho-mqtt3c -lgpiod -lpigpio -pthread -lrt

실행 : 
./Bcar_C
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h> // MQTTClient.h는 MQTT 클라이언트 라이브러리의 헤더 파일
#include <unistd.h>
#include "sensor.h"

#define ADDRESS "tcp://broker.hivemq.com:1883"
#define CLIENTID "Bcar_Container" // 다른 클라이언트 ID 사용 권장
#define TOPIC_B_DEST "storage/b_dest" // B차 목적지 토픽
#define TOPIC_B_DEST_ARRIVED "storage/b_dest_arrived" 
#define TOPIC_B_POINT        "storage/b_point"
#define TOPIC_B_POINT_ARRIVED   "storage/b_point_arrived"
#define TOPIC_B_COMPLETED "vehicle/B_completed"
#define QOS 1
#define TIMEOUT 10000L

MQTTClient client;

int waiting_for_arrival = 0;  // 0: 조회 가능, 1: 도착 대기 중
char current_zone[64] = {0}; // 현재 목적지 구역 ID를 저장하는 전역 변수  

// db_access.py에서 목적지 구역 ID를 가져오는 함수
char *B_destination()
{
    static char result[64];
    FILE *fp = popen("python3 -c \""
                     "from db_access import B_destination; "
                     "zone = B_destination(); "
                     "print(zone if zone else '')"
                     "\"",
                     "r");
    // popen : 쉘에서 파이썬 인터프리터를 실행

    if (fp == NULL)
    {
        fprintf(stderr, "Failed to run Python inline script\n");
        return NULL;
    }
    if (fgets(result, sizeof(result), fp) != NULL)
    {
        result[strcspn(result, "\n")] = 0; // 개행 제거
    }
    else
    {
        pclose(fp);
        return NULL;
    }
    pclose(fp);
    return result;
}

void publish_point()
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    const char *payload = "B차량 출발지점으로 출발";

    pubmsg.payload = (char *)payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, TOPIC_B_POINT, &pubmsg, &token);

    if (rc == MQTTCLIENT_SUCCESS)
    {
        printf("[송신] %s → %s\n", TOPIC_B_POINT, payload);
    }
    else
    {
        fprintf(stderr, "MQTT publish failed (출발 메시지), rc=%d\n", rc);
    }
}
// 메시지 송신 함수
void publish_zone(const char *구역_ID)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer; // 메세지 구조체 초기화
    char payload[64];

    // 페이로드 버퍼에 목적지 구역 ID를 복사
    snprintf(payload, sizeof(payload), "%s", 구역_ID);
    // snprintf : 문자열을 형식(format)에 맞게 만들어서 문자열 변수에 저장할 수 있도록 해주는 함수

    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    // MQTTClient_publishMessage : MQTT 메시지를 발행하는 함수
    int rc = MQTTClient_publishMessage(client, TOPIC_B_DEST, &pubmsg, &token);

    if (rc == MQTTCLIENT_SUCCESS)
    {
        // MQTTClient_waitForCompletion(client, token, TIMEOUT);
        printf("[송신] %s → %s로 이동\n", TOPIC_B_DEST, 구역_ID);
    }
    else
    {
        fprintf(stderr, "MQTT publish failed, rc=%d\n", rc);
    }
}
// 메시지 수신 함수
int message_arrived(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    char msg[message->payloadlen + 1];
    memcpy(msg, message->payload, message->payloadlen);
    msg[message->payloadlen] = '\0';

    printf("[수신] 토픽: %s, 메시지: %s\n", topicName, msg);

    //move_servo(chip, 0); // 수정필요!! B차 구역함 도착시 임의로 02(0) 구역함 서보모터 동작
                         // 나중엔 해당 구역번호를 받아서 해당 센서만 동작해야함!!

    if(strcmp(topicName,TOPIC_B_DEST_ARRIVED)==0)
    {
        char msgPayload[512]; // 예: "02 도착"
        strncpy(msgPayload, message->payload, message->payloadlen);
        msgPayload[message->payloadlen] = '\0';

        char zone_id = msgPayload[0];  // 문자 하나
        char zone_id_str[2] = { zone_id, '\0' };  // 문자열로 변환

        int servo_idx = -1;
        // zone_id → 서보 인덱스 매핑
        switch (zone_id) {
            case 'S': servo_idx = 0; break;  // S 구역 → 0번 서보
            case 'G': servo_idx = 1; break;  // G 구역 → 1번 서보
            case 'K': servo_idx = 2; break;  // K 구역 → 2번 서보
            case 'W': servo_idx = 3; break;  // W 구역 → 3번 서보
            default:
                fprintf(stderr, "Unknown zone_id '%c'\n", zone_id);
                break;
        }

        if (servo_idx >= 0) {
            printf("[서보] zone_id='%c' → servo %d 작동\n",
                   zone_id, servo_idx+1);
            // sensor.c 의 servo_once(idx) 를 호출하는 래퍼
            sensor_activate_servo(servo_idx);
        }

        char cmd2[512];
        snprintf(cmd2, sizeof(cmd2),
                "python3 - << 'EOF'\n"
                "from db_access import get_connection, zone_arrival_B\n"
                "conn = get_connection()\n"
                "cur = conn.cursor()\n"
                "zone_arrival_B(conn, cur, '%s', '%s')\n"
                "conn.close()\n"
                "EOF",
                zone_id_str,
                "B-1001");
        if (system(cmd2) != 0)
        {
            fprintf(stderr, "❌ zone_arrival_B() 실행 실패 (zone=%s)\n", msg);
        }
        else
        {
            printf("✅ zone_arrival_B() 실행 완료\n");
            publish_zone("B");// 
        }
    }
    
    if(strcmp(topicName,TOPIC_B_POINT_ARRIVED)==0)
    {
        publish_zone(current_zone);    // 목적지 zone ID 발행
    }
    if(strcmp(topicName, TOPIC_B_COMPLETED) == 0)
    {
        // 다음 목적지 조회를 가능하게 설정
        waiting_for_arrival = 0;
        // printf("B차량이 구역함에서 나갔습니다.\n");
        // B차량이 구역함에서 나갔다는 통신이 오면 서보모터를 닫는 로직을 추가해야 합니다.
        // move_servo(chip, 0); // 수정필요!! B차 구역함 나감시 임의로 02(0) 구역함 서보모터 동작
    }
    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);
    return 1;
}



// 콜백: 연결 끊겼을 때 호출됩니다
void connection_lost(void *context, char *cause)
{
    fprintf(stderr, "[경고] MQTT 연결 끊김: %s\n", cause);
}
int main(int argc, char *argv[])
{
    sensor_init();

    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    // 전역 clinet 객체를 생성
    MQTTClient_create(&client, ADDRESS, CLIENTID,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_setCallbacks(client, NULL, connection_lost,
                            message_arrived, NULL);
    // 브로커에 연결
    if (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "Failed to connect to MQTT broker\n");
        return -1;
    }
    printf("Connected to MQTT broker at %s\n", ADDRESS);


    MQTTClient_subscribe(client, TOPIC_B_DEST_ARRIVED, QOS);
    MQTTClient_subscribe(client, TOPIC_B_POINT_ARRIVED, QOS);
    MQTTClient_subscribe(client, TOPIC_B_COMPLETED, QOS);
    
    int rc1 = MQTTClient_subscribe(client, TOPIC_B_DEST_ARRIVED, QOS);
    if (rc1 != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "ERROR: TOPIC_B_DEST_ARRIVED 구독 실패, rc=%d\n", rc1);
        return -1;
    }
    int rc2 = MQTTClient_subscribe(client, TOPIC_B_POINT_ARRIVED, QOS);
    if (rc2 != MQTTCLIENT_SUCCESS) {
        fprintf(stderr, "ERROR: TOPIC_B_HOME_ARRIVED 구독 실패, rc=%d\n", rc2);
        return -1;
    }

    
    char prev_zone[64] = {0};
    // 주기적 또는 이벤트 기반 호출 예시
    while (1)
    {
        // ─────────────────────────────────────────────
        // ① 네트워크 I/O 처리 (메시지 수신 콜백을 실행시키기 위해)
        MQTTClient_yield();

        if (!waiting_for_arrival)
        {
            char *zone = B_destination();
            if (zone && *zone && strcmp(zone, prev_zone) != 0)
            {
                publish_point();       // 출발지점 메시지 발행

                strncpy(current_zone, zone, sizeof(current_zone) - 1); // 현재 목적지를 전역 변수에 저장
                current_zone[sizeof(current_zone) - 1] = '\0';
                // publish_zone(zone);    // 목적지 zone ID 발행
                strncpy(prev_zone, zone, sizeof(prev_zone) - 1);
                prev_zone[sizeof(prev_zone) - 1] = '\0';

                waiting_for_arrival = 1;  // 출발 이후에는 도착 대기 상태로 전환
            }
        }

        sleep(5); // 5초 간격으로 폴링
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);
    sensor_cleanup(); 
    return 0;
}