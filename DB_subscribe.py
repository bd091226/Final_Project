# DB_subscribe.py
import paho.mqtt.client as mqtt

# MQTT 브로커 설정 (publisher와 동일하게)
BROKER      = "broker.hivemq.com"
PORT        = 1883
MQTT_TOPIC = 'vehicle/B/start'
MQTT_CLIENT_ID = 'vehicle_b_subscriber'

# 콜백: 연결 시 토픽 구독
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("MQTT 브로커 연결 성공")
        client.subscribe(MQTT_TOPIC)
        print(f"Subscribed to topic: {MQTT_TOPIC}")
    else:
        print(f"Failed to connect, return code {rc}")

# 콜백: 메시지 수신 시 호출
def on_message(client, userdata, msg):
    payload = msg.payload.decode('utf-8')
    print(f"*** B차 시동 메시지 수신: {payload} ***")
    # 필요 시 추가 동작 수행 (예: 모터 구동 신호)

if __name__ == '__main__':
    client = mqtt.Client(client_id=MQTT_CLIENT_ID)
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(BROKER, PORT, keepalive=60)
    print("B차 MQTT 수신 대기 중...")
    client.loop_forever()
