#include "sensor.h" // 초음파 센서와 관련 함수
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h>
#include <unistd.h>

#define ADDRESS "tcp://broker.hivemq.com:1883"
#define CLIENTID "RaspberryPi_Container"            // 다른 클라이언트 ID 사용 권장
#define TOPIC_COUNT "storage/count"                 // count 값 수신
#define TOPIC_A_STARTPOINT  "storage/startpoint"       // 출발지점 출발 알림용 토픽 ("출발 지점으로 출발")
#define TOPIC_A_STARTPOINT_ARRIVED  "storage/startpoint_arried"       // 출발지점 도착 알림용 토픽 ("출발지점 도착")
#define TOPIC_A_DEST "storage/dest"                 // 목적지 구역 송신 토픽
#define TOPIC_A_DEST_ARRIVED "storage/arrived"           // 목적지 도착 메시지 수신 토픽
#define TOPIC_A_HOME "storage/home"                 // A차 집으로 출발 메시지 송신 토픽
#define QOS 1
#define TIMEOUT 10000L

MQTTClient client;          // MQTT 클라이언트 전역 변수
volatile int connected = 0; // 연결 여부 확인

// Python에서 A차의 다음 목적지 구역 ID 가져오기
char *A_destination(const char *운행_id)
{
    static char result[64]; //  출력 결과를 저장
    char cmd[256];          // popen에게 전달할 python 명령어를 저장

    // db_access.py 모듈에서 A_destinatin 함수 호출 후 결과 출력
    snprintf(cmd, sizeof(cmd),
             "python3 -c \"from db_access import A_destination; "
             "zone = A_destination('%s'); "
             "print(zone if zone else '')\"",
             운행_id);

    // 출력 스트림을 읽기 모드로 오픈
    FILE *fp = popen(cmd, "r");
    if (fp == NULL)
    {
        // 실패시 에러메세지
        fprintf(stderr, "Failed to run Python inline script\n");
        return NULL;
    }
    // 결과를 읽어서 개행 문자를 제거
    if (fgets(result, sizeof(result), fp) != NULL)
    {
        result[strcspn(result, "\n")] = 0;
    }
    else
    {
        // 출력이 없으면 popen 닫음
        pclose(fp);
        return NULL;
    }
    pclose(fp);
    return result; // 구역 ID 문자열 반환
}

// A차가 다음에 이동해야할 구역 ID를 MQTT 토픽에 송신
void publish_zone(const char *구역_ID)
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer; // MQTT 메시지 초기화
    char payload[64];                                           // 송신할 페이로드 문자열

    // 페이로드에 구역ID 복사
    snprintf(payload, sizeof(payload), "%s", 구역_ID);

    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token; // 발송 완료 토큰

    // A차가 이동해야하는  목적지 구역 송신
    int rc = MQTTClient_publishMessage(client, TOPIC_A_DEST, &pubmsg, &token);

    if (rc == MQTTCLIENT_SUCCESS)
    {
        printf("[송신] %s → %s\n", TOPIC_A_DEST, 구역_ID);
    }
    else
    {
        fprintf(stderr, "MQTT publish failed, rc=%d\n", rc);
    }
}

// A차가 다음 이동해야하는 구역 ID를 MQTT 토픽에 송신
void publish_home_message()
{
    MQTTClient_message pubmsg = MQTTClient_message_initializer; // MQTT 메시지 초기화
    const char *payload = "집으로 출발";                        // 송신할 문자열

    // pubmsg 구조체에 페이로드 및 속성 설정
    pubmsg.payload = (char *)payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token; // 송신 완료 토큰
    // "집으로 출발" 송신
    int rc = MQTTClient_publishMessage(client, TOPIC_A_HOME, &pubmsg, &token);
    if (rc == MQTTCLIENT_SUCCESS)
    {
        MQTTClient_waitForCompletion(client, token, TIMEOUT);
        // 송신 성공 시 콘솔에 출력
        printf("[송신] %s → %s\n", TOPIC_A_HOME, payload);
    }
    else
    {
        // 송신 실패 시 에러 출력
        fprintf(stderr, "MQTT publish failed (rc=%d)\n", rc);
    }
}


// 수신받았을 때 호출되는 함수
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    char *payloadptr = (char *)message->payload; // 수신된 메세지 데이터의 포인터

    // 수신 메시지를 문자열로 복사
    char msgPayload[message->payloadlen + 1];
    memcpy(msgPayload, payloadptr, message->payloadlen);
    msgPayload[message->payloadlen] = '\0'; // 마지막에 널 종료 문자 추가

    // 수신된 내용 터미널에 출력
    printf("메시지 수신: [%s] → %s\n", topicName, msgPayload);

    // 수신한 토픽이 storage/count일 경우
    if (strcmp(topicName, TOPIC_COUNT) == 0)
    {
        int count = atoi(msgPayload); // 문자열을 정수로 변환

        // DB -> button_A 실행
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "python3 - << 'EOF'\n"
                 "from db_access import get_connection, button_A\n"
                 "conn = get_connection()\n"
                 "cur = conn.cursor()\n"
                 "button_A(cur, conn, %d, \"%s\")\n"
                 "conn.close()\n"
                 "EOF",
                 count,
                 "A-1000" // 기존 운행_ID // 수정요청
        );

        // system() 호출하여 쉘에서 파이썬 스크립트 실행
        int ret = system(cmd);
        if (ret == -1)
        {
            fprintf(stderr, "❌ Python button_A 실행 실패\n");
        }
        else
        {
            printf("✅ Python button_A 실행 완료 (count=%d)\n", count);
        }

        // count의 값이 2보다 클 경우에 출발 지점으로 출발하라는 문구 송신
        if (count > 2)
        {
            MQTTClient_message startMsg=MQTTClient_message_initializer;
            const char *startPayload="출발지점으로 출발";
            startMsg.payload=(char*)startPayload;
            startMsg.payloadlen = (int)strlen(startPayload);
            startMsg.qos = QOS;
            startMsg.retained = 0;
            
            MQTTClient_deliveryToken startToken;
            int rc_start = MQTTClient_publishMessage(client,TOPIC_A_STARTPOINT,&startMsg, &startToken);
            if(rc_start==MQTTCLIENT_SUCCESS)
            {
                printf("[송신] %s → %s\n", TOPIC_A_STARTPOINT, startPayload);
            }
            else{
                printf("MQTT publish failed (rc=%d)\n", rc_start);
            }
        }
    }

//TOPIC_A_destination로 A차가 목적지로 출발했다는 메세지를 수신
    if (strcmp(topicName, TOPIC_A_STARTPOINT) == 0)
    {
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
                    'A-1000'); // 수정 요청

        // system으로 departed_A발행
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

    if(strcmp(topicName, TOPIC_A_STARTPOINT_ARRIVED) == 0)
    {
        // 차량_ID를 임의로 지정하여 나중에 변경
            char *zone = A_destination("1000");

            if (zone && *zone)
            {
                // 조회된 구역ID가 있을 때만 해당 ID 송신
                publish_zone(zone);
            }
            else
            {
                // 조회된 구역 ID가 없으면
                publish_zone("02"); // 임의로 넣어놓음, 나중에 삭제 요청바람
                printf("조회된 구역이 없습니다.\n");
            }

    }
    if(strcmp(topicName, TOPIC_A_DEST_ARRIVED) == 0)
    {
        // 도착 알림 수신 확인 메시지 터미널에 출력
        printf("✅ A차가 목적지에 도착했습니다. 필요한 동작을 수행하세요.\n");
        // 1) 초음파 센서 실행
        float prev_distance = 0;
        if (move_distance(chip, 0, &prev_distance))
        {
            // 초음파 센서가 true 이면 zone_arrival_A() 호출 (DB에 도착 처리)
            char cmd_zone[512];
            snprintf(cmd_zone, sizeof(cmd_zone),
                        "python3 - << 'EOF'\n"
                        "from db_access import get_connection, zone_arrival_A\n"
                        "conn = get_connection()\n"
                        "cur = conn.cursor()\n"
                        "zone_arrival_A(conn, cur, %d, '%s')\n"
                        "conn.close()\n"
                        "EOF",
                        1,   // 차량_ID = 1 // 수정 요청
                        "02" // 구역_ID ("02"로 고정, 추후 동적으로 변경) // 수정 요청
            );

            // system으로 zone_arrival_A발행
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
            //    - 결과가 0이면 "집으로 출발" 메시지를 MQTT로 송신
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
                        "A-1000" // 실제 차량_ID에 맞게 변경하세요 // 수정 요청
            );

            // popen()을 사용해 Python 출력(=적재 수량)을 읽기모드
            FILE *fp = popen(cmd_count, "r");
            if (fp == NULL)
            {
                fprintf(stderr, "❌ get_A_count() popen 호출 실패\n");
            }
            else
            {
                int load_count = -1;                    // 적재 수량 저장 변수
                if (fscanf(fp, "%d", &load_count) == 1) // popen 출력을 정수로 읽음
                {
                    printf("🔍 현재 A차 적재 수량: %d\n", load_count); // 현재 적재 수량 출력
                    if (load_count == 0)                               // 적재 수량이 0일때
                    {
                        // TOPIC_A_HOME으로 "집으로 출발" 송신
                        publish_home_message();
                    }
                    else
                    // 현재 적재 수량이 0이 아닐때는 A_destination함수를 호출하여 다음 구역 ID을 받아옴
                    {
                        const char *운행_ID = "1000"; // 수정 요청
                        char *next_zone = A_destination(운행_ID);
                        if (next_zone && *next_zone)
                        {
                            printf(" 다음 목적지 구역 : %s\n", next_zone);
                            publish_zone(next_zone);
                        }
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
   

    // 수신한 토픽이 storage/arrived일 경우
    // if (strcmp(topicName, TOPIC_A_DEST) == 0)
    // {
    //     // 페이로드가 "A차 목적지 도착" 문자열인지 확인
    //     if (strcmp(msgPayload, "A차 목적지 도착") == 0)
    //     {
    //         
    //     }

        

        MQTTClient_freeMessage(&message);
        MQTTClient_free(topicName);

        return 1;
    
}

// 브로커와 연결 끊겼을 때 호출되는 콜백 함수
void connlost(void *context, char *cause)
{
    // 연결 끊김 정보 출력
    printf("Connection lost: %s\n", cause);
    connected = 0;
}
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    // 메시지 발송 완료 콜백 (필요시 사용)
}

int main(int argc, char *argv[])
{
    // 연결 옵션 구조체 초기화
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    // MQTT 클라이언트 생성
    MQTTClient_create(&client, ADDRESS, CLIENTID, MQTTCLIENT_PERSISTENCE_NONE, NULL);

    // 콜백함수등록
    MQTTClient_setCallbacks(client, NULL, connlost, msgarrvd, delivered);
    // connlost : 연결 끊김 콜백
    // msgarrvd : 메시지 수신 콜백
    // delivered : 메시지 발송 완료 콜백

    // MQTT 브로커에 연결 시도
    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS)
    {
        // 실패시 에러메시지
        printf("Failed to connect, return code %d\n", rc);
        return -1;
    }

    // 연결 성공시 출력
    printf("Connected to MQTT broker, subscribing to topic: %s\n", TOPIC_COUNT);

    // 구독할 토픽 등록
    MQTTClient_subscribe(client, TOPIC_COUNT, QOS);
    MQTTClient_subscribe(client, TOPIC_A_DEST_ARRIVED, QOS);
    MQTTClient_subscribe(client, TOPIC_A_STARTPOINT, QOS);
    MQTTClient_subscribe(client, TOPIC_A_STARTPOINT_ARRIVED, QOS);
    MQTTClient_subscribe(client, TOPIC_A_DEST, QOS);
    

    // 메시지 수신을 계속 대기 (무한 루프)
    while (1)
    {
        sleep(1); // Linux에서는 이것만 필요
    }

    MQTTClient_disconnect(client, 10000); // 연결해제
    MQTTClient_destroy(&client);          // 클라이언트 객체 소멸

    return 0;
}
