#include "sensor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h>
#include <unistd.h>

#define ADDRESS "tcp://broker.hivemq.com:1883"
#define CLIENTID "RaspberryPi_Container"            // 다른 클라이언트 ID 사용 권장
#define TOPIC_COUNT "storage/count"                 // count 값 수신
#define TOPIC_A_destination "storage/start"         // 출발 알림 수신용 토픽
#define TOPIC_A_destinationDEST "storage/startdest" // 목적지 구역 송신 토픽
#define TOPIC_A_ARRIVED "storage/arrived"           // 목적지 도착 메시지 수신 토픽
#define TOPIC_A_HOME       "storage/home"
#define QOS 1
#define TIMEOUT 10000L

MQTTClient client;

volatile int connected = 0; // 연결 여부 확인
static int last_운행_ID = 0;

// Python에서 구역 ID 가져오기
char *A_destination(const char *unhaeng_id)
{
    static char result[64];
    char cmd[256];

    snprintf(cmd, sizeof(cmd),
             "python3 -c \"from db_access import A_destination; "
             "zone = A_destination('%s'); "
             "print(zone if zone else '')\"",
             unhaeng_id);

    FILE *fp = popen(cmd, "r");
    if (fp == NULL)
    {
        fprintf(stderr, "Failed to run Python inline script\n");
        return NULL;
    }
    if (fgets(result, sizeof(result), fp) != NULL)
    {
        result[strcspn(result, "\n")] = 0;
    }
    else
    {
        pclose(fp);
        return NULL;
    }
    pclose(fp);
    return result;
}
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    // 메시지 발송 완료 콜백 (필요시 사용)
}

void publish_zone(const char *구역_ID)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    char payload[64];

    snprintf(payload, sizeof(payload), "%s", 구역_ID);

    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    // A차가 이동해야하는  목적지 구역 발행
    int rc = MQTTClient_publishMessage(client, TOPIC_A_destinationDEST, &pubmsg, &token);

    if (rc == MQTTCLIENT_SUCCESS)
    {
        printf("[발행] %s → %s\n", TOPIC_A_destinationDEST, 구역_ID);
    }
    else
    {
        fprintf(stderr, "MQTT publish failed, rc=%d\n", rc);
    }
}

// 메시지가 도착 했을때 호출 되는 것
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    char *payloadptr = (char *)message->payload;

    // 수신 메시지를 문자열로 복사
    char msgPayload[message->payloadlen + 1];
    memcpy(msgPayload, payloadptr, message->payloadlen);
    msgPayload[message->payloadlen] = '\0';

    printf("메시지 수신: [%s] → %s\n", topicName, msgPayload);

    // 수신한 토픽이 storage/count일 경우
    if (strcmp(topicName, TOPIC_COUNT) == 0)
    {
        int count = atoi(msgPayload);
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "python3 - << 'EOF'\n"
                 "from db_access import get_connection, button_A\n"
                 "conn = get_connection()\n"
                 "cur = conn.cursor()\n"
                 "button_A(cur, conn, %d, %d)\n"
                 "conn.close()\n"
                 "EOF",
                 count,
                 1001 // 기존 운행_ID
        );
        int ret = system(cmd);
        if (ret == -1)
        {
            fprintf(stderr, "❌ Python button_A 실행 실패\n");
        }
        else
        {
            printf("✅ Python button_A 실행 완료 (count=%d)\n", count);
        }

        if (count > 2)
        {
            // 차량_ID를 임의로 지정하여 나중에 변경
            char *zone = A_destination("1001");

            if (zone && *zone)
            {
                publish_zone(zone);
            }
            else
            {
                publish_zone("02");
                printf("조회된 구역이 없습니다.\n");
            }
        }
    }
    // 수신한 토픽이 storage/arrived일 경우
    if (strcmp(topicName, TOPIC_A_ARRIVED) == 0)
    {
        if (strcmp(msgPayload, "A차 목적지 도착") == 0)
        {
            printf("✅ A차가 목적지에 도착했습니다. 필요한 동작을 수행하세요.\n");
            // 1) 기존에 작성한 초음파 센서 로직 실행 (예시)
            float prev_distance = 0;
            if (move_distance(chip, 0, &prev_distance))
            {
                // zone_arrival_A() 호출 (DB에 도착 처리)
                char cmd_zone[512];
                snprintf(cmd_zone, sizeof(cmd_zone),
                        "python3 - << 'EOF'\n"
                        "from db_access import get_connection, zone_arrival_A\n"
                        "conn = get_connection()\n"
                        "cur = conn.cursor()\n"
                        "zone_arrival_A(conn, cur, %d, '%s')\n"
                        "conn.close()\n"
                        "EOF",
                        1,   // 차량_ID = 1
                        "02" // 구역_ID ("02"로 고정, 추후 동적으로 변경)
                );

                int ret_zone = system(cmd_zone);
                if (ret_zone != 0)
                {
                    fprintf(stderr, "❌ zone_arrival_A() 실행 실패 (rc=%d)\n", ret_zone);
                }
                else
                {
                    printf("✅ zone_arrival_A() 실행 완료\n");
                }

                // 2) Python get_A_count() 호출 → 현재 적재 수량 받아오기
                //    - get_A_count(cursor, 차량_ID='A-1000') 함수를 이용
                //    - 결과가 0이면 "집으로 출발" 메시지를 MQTT로 발행
                char cmd_count[1024];
                snprintf(cmd_count, sizeof(cmd_count),
                        "python3 - << 'EOF'\n"
                        "from db_access import get_connection, get_A_count\n"
                        "conn = get_connection()\n"
                        "cur = conn.cursor()\n"
                        "count = get_A_count(cur, '%s')\n"
                        "print(count)\n"
                        "conn.close()\n"
                        "EOF",
                        "A-1000"  // 실제 차량_ID에 맞게 변경하세요
                );

                // popen()을 사용해 Python 출력(=적재 수량)을 읽어들임
                FILE *fp = popen(cmd_count, "r");
                if (fp == NULL)
                {
                    fprintf(stderr, "❌ get_A_count() popen 호출 실패\n");
                }
                else
                {
                    int load_count = -1;
                    if (fscanf(fp, "%d", &load_count) == 1)
                    {
                        printf("🔍 현재 A차 적재 수량: %d\n", load_count);
                        if (load_count == 0)
                        {
                            // 3) 적재 수량이 0이면 TOPIC_A_HOME으로 "집으로 출발" 발행
                            MQTTClient_message pubmsg = MQTTClient_message_initializer;
                            pubmsg.payload = "집으로 출발";
                            pubmsg.payloadlen = (int)strlen("집으로 출발");
                            pubmsg.qos = QOS;
                            pubmsg.retained = 0;

                            MQTTClient_deliveryToken token;
                            int rc = MQTTClient_publishMessage(client,TOPIC_A_HOME,&pubmsg,&token);
                            if (rc != MQTTCLIENT_SUCCESS)
                            {
                                fprintf(stderr, "❌ MQTT publish 실패 (rc=%d)\n", rc);
                            }
                            else
                            {
                                // 블로킹으로 완료 대기
                                MQTTClient_waitForCompletion(client, token, TIMEOUT);
                                printf("✅ '집으로 출발' 메시지 발행 완료 (토픽: %s)\n", TOPIC_A_HOME);
                            }
                        }
                        else // 현재 적재 수량이 0이 아닐때는 db_access.py에서 A_destination함수를 호출하여 구역 ID를 새로 받아옴
                        {
                            // char cmd_dest[1024];
                            // snprintf(cmd_dest, sizeof(cmd_dest),
                            //         "python3 - << 'EOF'\n"
                            //         "from db_access import A_destination\n"
                            //         "zone = A_destination(%d)\n"
                            //         "if zone:\n"
                            //         "    print(zone)\n"
                            //         "EOF",
                            //         운행_ID
                            // );
                        }
                    }
                    else
                    {
                        fprintf(stderr, "❌ get_A_count() 출력 파싱 실패\n");
                    }

                    pclose(fp);
                }
            }
            else
            {
                printf("🔕 센서 조건 미충족 (거리 > 15cm 또는 변화 < 5cm), DB 호출 생략\n");
            }
        }

        // A차가 목적지로 출발했다는 메세지를 수신
        if (strcmp(topicName, TOPIC_A_destination) == 0)
        {
            // "~로 출발했음" 메시지 수신 처리
            printf("출발 알림 수신: %s\n", msgPayload);

            char cmd[512];
            // 차량_ID를 1로 고정. 필요하면 msgPayload에서 파싱해 넣어도 됩니다.
            snprintf(cmd, sizeof(cmd),
                     "python3 - << 'EOF'\n"
                     "from db_access import get_connection, departed_A\n"
                     "conn = get_connection()\n"
                     "cur = conn.cursor()\n"
                     "departed_A(conn, cur, %s)\n"
                     "conn.close()\n"
                     "EOF",
                     'A-1000'); // 수정
            int ret = system(cmd);
            if (ret != 0)
            {
                fprintf(stderr, "❌ departed_A() 실행 실패 (rc=%d)\n", ret);
            }
            else
            {
                printf("✅ departed_A() 실행 완료\n");
            }
        }

        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);

        return 1;
    }
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

    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);
    // connlost : 연결 끊김 콜백
    // msgarrvd : 메시지 수신 콜백
    // delivered : 메시지 발송 완료 콜백

    // 이 라인 추가
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        printf("Failed to connect, return code %d\n", rc);
        return -1;
    }

    // 연결 성공시 출력
    printf("Connected to MQTT broker, subscribing to topic: %s\n", TOPIC_COUNT);

    MQTTClient_subscribe(client, TOPIC_COUNT, QOS);
    MQTTClient_subscribe(client, TOPIC_A_ARRIVED, QOS);
    MQTTClient_subscribe(client, TOPIC_A_destination, QOS);

    // 메시지 수신을 계속 대기 (무한 루프)
    while (1)
    {
        sleep(1); // Linux에서는 이것만 필요
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return 0;
}
