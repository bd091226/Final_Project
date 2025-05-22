import paho.mqtt.client as mqtt
from container_config import (
    BROKER, PORT,
    TOPIC_SUB,        # 버튼 카운트
    TOPIC_PUB,        # A차 출발
    TOPIC_PUB_DIST,   # B차 출발 알림 (구독만 함)
    TOPIC_STATUS
)
from container_DB import update_load_count, handle_qr_insert

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        client.subscribe(TOPIC_SUB, qos=1)
        client.subscribe(TOPIC_PUB_DIST, qos=1)
        client.subscribe(TOPIC_STATUS, qos=1)
        print("✅ MQTT 연결 및 구독 완료")
    else:
        print(f"❌ MQTT 연결 실패: 코드 {rc}")

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode().strip()
    conn, cursor = userdata['db']

    if topic == TOPIC_SUB:
        try:
            count = int(payload)
            update_load_count(cursor, conn, count)
            if count > 5:
                client.publish(TOPIC_PUB, "A차 출발", qos=1)
                print(f"🚗 A차 출발 메시지 발행 → {TOPIC_PUB}")
        except ValueError:
            print("❌ 잘못된 숫자 payload")

    elif topic == TOPIC_PUB_DIST:
        # 이제 거리값이 아님 → 상태 메시지
        print(f"📥 B차 측 거리 조건 충족 메시지 수신: '{payload}'")
        # 여기에 상태 저장 로직 추가하고 싶으면 별도 함수 만들기

    elif topic == TOPIC_STATUS:
        print(f"📥 B차 상태 메시지 수신: '{payload}'")
        if payload == "목적지 도착":
            print("🎯 B차가 목적지에 도착했습니다!")

    else:
        print(f"⚠️ 처리되지 않은 토픽: {topic}")

def create_mqtt_client(db_conn_tuple):
    client = mqtt.Client(userdata={'db': db_conn_tuple})
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, keepalive=60)
    return client
