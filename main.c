#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiringPi.h>
#include <MQTTClient.h>

#define ADDRESS     "tcp://broker.hivemq.com:1883"  // 공용 MQTT 브로커 예시 (변경 가능)
#define CLIENTID    "RaspberryPi_A"
#define TOPIC       "storage/count"
#define QOS         1
#define TIMEOUT     10000L

#define BUTTON_PIN  0  // wiringPi 핀번호 (예: GPIO17)

MQTTClient client;
int count = 0;

void send_count() {
    char payload[50];
    sprintf(payload, "%d", count);

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, TOPIC, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS) {
        printf("Failed to publish message, return code %d\n", rc);
        return;
    }
    rc = MQTTClient_waitForCompletion(client, token, TIMEOUT);
    printf("Message with count %d delivered\n", count);
}

int main() {
    int rc;

    wiringPiSetup();
    pinMode(BUTTON_PIN, INPUT);
    pullUpDnControl(BUTTON_PIN, PUD_UP);  // 풀업저항 설정

    MQTTClient_create(&client, ADDRESS, CLIENTID,
                      MQTTCLIENT_PERSISTENCE_NONE, NULL);
    MQTTClient_connectOptions conn_opts = MQTTClient_connectOptions_initializer;

    if ((rc = MQTTClient_connect(client, &conn_opts)) != MQTTCLIENT_SUCCESS) {
        printf("Failed to connect to MQTT broker, return code %d\n", rc);
        return -1;
    }

    printf("MQTT connected. Waiting for button press...\n");

    int lastButtonState = HIGH;
    while (1) {
        int buttonState = digitalRead(BUTTON_PIN);
        if (lastButtonState == HIGH && buttonState == LOW) {
            // 버튼 눌림 감지
            count++;
            printf("Button pressed! Sending count: %d\n", count);
            send_count();
        }
        lastButtonState = buttonState;
        delay(100); // 100ms 딜레이 (디바운싱)
    }

    MQTTClient_disconnect(client, 10000);
    MQTTClient_destroy(&client);

    return 0;
}
