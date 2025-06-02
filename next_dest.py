# next_dest.py
import paho.mqtt.client as mqtt

# MQTT 브로커 설정 (publisher와 동일하게)
BROKER      = "broker.hivemq.com"
PORT        = 1883
TOPIC_A_NEXT = 'vehicle/A/next'

current_destination = None
# 콜백: 연결 시 토픽 구독
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("MQTT 브로커 연결 성공")
        client.subscribe(TOPIC_A_NEXT)
        print(f"Subscribed to topic: {TOPIC_A_NEXT}")
    else:
        print(f"Failed to connect, return code {rc}")

# 콜백: 메시지 수신 시 호출
def on_message(client, userdata, msg):
    global current_destination
    payload = msg.payload.decode('utf-8')
    current_destination = payload
    print(f"*** 이번 목적지 : {current_destination} ***")
    # 필요 시 추가 동작 수행 (예: 모터 구동 신호)

def destination_listen():
    client = mqtt.Client()
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, keepalive=60)
    client.loop_start()  # 백그라운드로 실행되도록 loop_start 사용
    return client  # 필요시 client를 stop할 수 있도록 반환
