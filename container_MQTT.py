# container_MQTT.py

import paho.mqtt.client as mqtt
from container_config import (
    BROKER, PORT,
    TOPIC_SUB,        # 버튼 카운트
    TOPIC_PUB,        # A차 출발
    TOPIC_PUB_DIST    # C차 출발(거리 경고) <-- 새로 구독
)
from container_DB import update_load_count, insert_vehicle_status_B

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        client.subscribe(TOPIC_SUB, qos=1)
        client.subscribe(TOPIC_PUB_DIST, qos=1)
        print(f"👉 MQTT connected. Subscribed to {TOPIC_SUB} and {TOPIC_PUB_DIST}")
    else:
        print(f"❌ MQTT connect failed with code {rc}")

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode()
    conn, cursor = userdata['db']

    if topic == TOPIC_SUB:
        # (기존) 버튼 카운트 처리 → A차 출발 발행
        try:
            count = int(payload)
            update_load_count(cursor, conn, count)
            if count > 5:
                client.publish(TOPIC_PUB, "A차 출발", qos=1)
                print(f"🔄 Published 'A차 출발' to {TOPIC_PUB}")
        except ValueError:
            print("❌ Count payload is not an integer.")

    elif topic == TOPIC_PUB_DIST:
        # (새로) 거리 경고 수신 시 B차 상태 DB 기록
        print(f"📥 Received distance alert: {payload}")
        insert_vehicle_status_B(cursor, conn)
        print("🔄 Inserted new record into vehicle_status_B")

    else:
        print(f"⚠️ Unhandled topic: {topic}")

def create_mqtt_client(db_conn_tuple):
    client = mqtt.Client(userdata={'db': db_conn_tuple})
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, keepalive=60)
    return client
