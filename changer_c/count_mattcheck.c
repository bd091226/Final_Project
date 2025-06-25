/*
count_mattcheck.c
컴파일 :
gcc -pthread -o count_mattcheck count_mattcheck.c sensor.c -lgpiod -lpaho-mqtt3c -lm

실행 :
./count_mattcheck
*/

#include "sensor.h" // 초음파 센서와 관련 함수
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <MQTTClient.h>
#include <unistd.h>
#include <gpiod.h>
#define ADDRESS "tcp://broker.hivemq.com:1883"
#define CLIENTID "RaspberryPi_Container"                        // 다른 클라이언트 ID 사용 권장
#define TOPIC_COUNT "storage/count"                             // count 값 수신
#define TOPIC_A_STARTPOINT "storage/startpoint"                 // 출발지점 출발 알림용 토픽 ("출발 지점으로 출발")
#define TOPIC_A_STARTPOINT_ARRIVED "storage/startpoint_arrived" // 출발지점 도착 알림용 토픽 ("출발지점 도착")
#define TOPIC_A_DEST "storage/dest"                             // 목적지 구역 송신 토픽
#define TOPIC_A_DEST_ARRIVED "storage/dest_arrived" 
#define TOPIC_A_COMPLETE "storage/A_complete"           // 목적지 도착 메시지 수신 토픽

#define QOS 1
#define TIMEOUT 10000L

MQTTClient client;          // MQTT 클라이언트 전역 변수
volatile int connected = 0; // 연결 여부 확인

char trip_id_str[16]; // 전역 변수로 trip_id 문자열 선언

// 센서 쓰레드가 계속 돌아갈지 제어하는 플래그
static volatile bool sensor_thread_running = true;

void startpoint()
{
    MQTTClient_message startMsg = MQTTClient_message_initializer;
    const char *startPayload = "A차 출발지점으로 출발";
    startMsg.payload = (char *)startPayload;
    startMsg.payloadlen = (int)strlen(startPayload);
    startMsg.qos = QOS;
    startMsg.retained = 0;

    MQTTClient_deliveryToken startToken;
    int rc_start = MQTTClient_publishMessage(client, TOPIC_A_STARTPOINT, &startMsg, &startToken);
    if (rc_start == MQTTCLIENT_SUCCESS)
    {
        printf("[송신] %s → %s\n", TOPIC_A_STARTPOINT, startPayload);
    }
    else
    {
        printf("MQTT publish failed (rc=%d)\n", rc_start);
    }
}
// Python에서 A차의 다음 목적지 구역 ID 가져오기
char *A_destination(const char *운행_id)
{
    static char result[512]; //  출력 결과를 저장
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




// 수신받았을 때 호출되는 함수
int msgarrvd(void *context, char *topicName, int topicLen, MQTTClient_message *message)
{
    char *payloadptr = (char *)message->payload; // 수신된 메세지 데이터의 포인터

    // 수신 메시지를 문자열로 복사
    char msgPayload[message->payloadlen + 1];
    memcpy(msgPayload, payloadptr, message->payloadlen);
    msgPayload[message->payloadlen] = '\0'; // 마지막에 널 종료 문자 추가

    // 수신된 내용 터미널에 출력
    printf("[수신]: [%s] → %s\n", topicName, msgPayload);

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
                 "A-1000" // 기존 trip_id // 수정요청
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
            startpoint();
        }
    }

    if (strcmp(topicName, TOPIC_A_STARTPOINT_ARRIVED) == 0)
    {
        char cmd[512];
        // 차량_ID를 1로 고정. 필요하면 msgPayload에서 파싱해 넣어도 됩니다.
        snprintf(cmd, sizeof(cmd),
                 "python3 - << 'EOF'\n"
                 "from db_access import get_connection, departed_A\n"
                 "conn = get_connection()\n"
                 "cur = conn.cursor()\n"
                 "departed_A(conn, cur, '%s')\n"
                 "conn.close()\n"
                 "EOF",
                 "A-1000"); // 수정 요청

        // popen으로 trip_id 결과 읽기
        FILE *fp = popen(cmd, "r");
        if (fp == NULL)
        {
            fprintf(stderr, "❌ popen 실패\n");
            return 0;
        }
        char buffer[128];
        if (fgets(buffer, sizeof(buffer), fp) != NULL)
        {
            // trip_id를 문자열로 받은 후 정수로 변환
            int trip_id = atoi(buffer);
            printf("✅ departed_A 실행 후 trip_id: %d\n", trip_id);

            if (trip_id > 0)
            {
                // 정수 trip_id를 문자열로 변환하여 zone 조회
                
                snprintf(trip_id_str, sizeof(trip_id_str), "%d", trip_id);

                char *zone = A_destination(trip_id_str);
                if (zone && *zone)
                {
                    publish_zone(zone);
                }
                else
                {
                    printf("조회된 구역이 없습니다.\n");
                }
            }
            else
            {
                printf("❌ departed_A()가 유효한 trip_id를 반환하지 않았습니다.\n");
            }
        }
        else
        {
            fprintf(stderr, "❌ Python에서 trip_id를 읽는 데 실패했습니다.\n");
        }
        pclose(fp);
    }
    if (strcmp(topicName, TOPIC_A_DEST_ARRIVED) == 0)
    {
        // 도착 알림 수신 확인 메시지 터미널에 출력
        printf("✅ A차가 목적지에 도착했습니다. 필요한 동작을 수행하세요.\n");
        
        //여기 초음파 센서 조건 실행하는 걸 해놔야함
        //임시로 조건 맞으면업데이트한다고 출력만하게 해놓았는데
        //초음파 조건 맞으면 db 업데이트 하는 걸로 수정 필요!
        
        char msgPayload[512]; // 예: "02 도착"
        strncpy(msgPayload, message->payload, message->payloadlen);
        msgPayload[message->payloadlen] = '\0';

        char zone_id = msgPayload[0];  // 문자 하나
        char zone_id_str[2] = { zone_id, '\0' };  // 문자열로 변환

        char cmd_zone[512];
        snprintf(cmd_zone, sizeof(cmd_zone),
                    "python3 - << 'EOF'\n"
                    "from db_access import get_connection, zone_arrival_A\n"
                    "conn = get_connection()\n"
                    "cur = conn.cursor()\n"
                    "zone_arrival_A(conn, cur, '%s', '%s')\n"
                    "conn.close()\n"
                    "EOF",
                    "A-1000", // 차량_ID = 1 // 수정 요청
                    zone_id_str      // 구역_ID ("02"로 고정, 추후 동적으로 변경) // 수정 요청
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
            fprintf(stderr, "❌ get_6A_count() popen 호출 실패\n");
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
                    MQTTClient_message homeMsg = MQTTClient_message_initializer;
                    const char *homePayload = "A차 집으로 출발";
                    homeMsg.payload = (char *)homePayload;
                    homeMsg.payloadlen = (int)strlen(homePayload);
                    homeMsg.qos = QOS;
                    homeMsg.retained = 0;
                    MQTTClient_deliveryToken homeToken;
                    int rc_home = MQTTClient_publishMessage(client, TOPIC_A_COMPLETE, &homeMsg, &homeToken);
                    if (rc_home == MQTTCLIENT_SUCCESS)
                    {
                        printf("[송신] %s → %s\n", TOPIC_A_COMPLETE, homePayload);
                    }
                    else
                    {
                        fprintf(stderr, "MQTT publish failed (rc=%d)\n", rc_home);
                    }
                    
                }
                else
                // 현재 적재 수량이 0이 아닐때는 A_destination함수를 호출하여 다음 구역 ID을 받아옴
                {
                    char 운행_ID[16]; // 운행 ID를 저장할 문자열
                    snprintf(운행_ID, sizeof(trip_id_str), "%s", trip_id_str);
                    printf("운행 ID: %s\n", 운행_ID); // 운행 ID 출력 (디버깅용)
                    char *next_zone = A_destination(운행_ID);
                    if (next_zone == NULL) {
                        fprintf(stderr, "❌ A_destination() 결과가 NULL입니다\n");
                        // 필요 시 fallback 동작 수행
                    } 
                    else 
                    {
                        printf(" 다음 목적지 구역 : %s\n", next_zone);
                        // ✅ 여기에 조건 분기 적용
                        if (strcmp(next_zone, "") == 0 || strlen(next_zone) == 0) {
                            printf("✅ 모든 목적지를 방문 완료했습니다. 차량을 홈으로 복귀시킵니다.\n");
                        } else {
                            publish_zone(next_zone); // 다음 목적지로 이동 명령
                        }
                    }
                    // if (next_zone && *next_zone)
                    // {
                    //     printf(" 다음 목적지 구역 : %s\n", next_zone);
                    //     publish_zone(next_zone);
                    // }
                }
            }
            else
            {
                fprintf(stderr, "❌ get_A_count() 출력 파싱 실패\n");
            }

            pclose(fp);
        }
        
    }

    MQTTClient_freeMessage(&message);
    MQTTClient_free(topicName);

    return 1;
}

// 브로커와 연결 끊겼을 때 호출되는 콜백 함수
void connlost(void *context, char *cause)
{
    // 연결 끊김 정보 출력
    printf("Connection lost: %s\n", cause);
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    conn_opts.keepAliveInterval = 20;
    conn_opts.cleansession = 1;

    while (MQTTClient_connect(client, &conn_opts) != MQTTCLIENT_SUCCESS) {
        printf("재접속 실패, 5초 후 재시도...\n");
        sleep(5);
    }
    printf("재접속 성공\n");
}
void delivered(void *context, MQTTClient_deliveryToken dt)
{
    // 메시지 발송 완료 콜백 (필요시 사용)
}

// 센서 전용 쓰레드 함수
static void *sensor_thread_fn(void *arg) {
    sensor_init();  // GPIO 초기화

    // 거리 ≤ 20cm, 변화량 ≥ 3cm, 0.2초 간격으로 모니터링
    sensor_monitor_triggers(20.0f, 3.0f, 200000, &sensor_thread_running);

    sensor_cleanup();  // GPIO 정리
    return NULL;
}

int main(int argc, char *argv[])
{
    // 시그널 핸들러 등록
    signal(SIGINT, handle_sigint);

     // 1) 센서 쓰레드 시작
    pthread_t thr;
    pthread_create(&thr, NULL, sensor_thread_fn, NULL);

    // 연결 옵션 구조체 초기화
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;
    int rc;

    //init_gpio_chip(); // sensor.c에서 정의된 함수

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
    // MQTTClient_subscribe(client, TOPIC_A_STARTPOINT, QOS);
    MQTTClient_subscribe(client, TOPIC_A_STARTPOINT_ARRIVED, QOS);
    MQTTClient_subscribe(client, TOPIC_A_DEST, QOS);
    

    // 메시지 수신을 계속 대기 (무한 루프)
    while (1)
    {
        sleep(1); // Linux에서는 이것만 필요
    }

    // 종료 시 센서 쓰레드 정리
    sensor_thread_running = false;
    pthread_join(thr, NULL);

    MQTTClient_disconnect(client, 10000); // 연결해제
    MQTTClient_destroy(&client);          // 클라이언트 객체 소멸

    return 0;
}
