# container_MQTT.py

import time
import paho.mqtt.client as mqtt

from container_config import (
    BROKER,
    PORT,
    TOPIC_SUB,       # 버튼 카운트 수신용 토픽
    TOPIC_PUB,       # A차 출발 명령 발행용 토픽
    TOPIC_PUB_DIST   # C차 출발(거리 경고) 명령 발행용 토픽
)
from container_DB import connect_db, update_load_count, insert_distance
from container_Sensor import setup, measure_distance, cleanup

# ─── MQTT Handler ───────────────────────────────────────────────
def on_connect(client, userdata, flags, rc):
    """
    Subscribe to the button-count topic on connect.
    """
    if rc == 0:
        client.subscribe(TOPIC_SUB, qos=1)
        print(f"👉 MQTT connected. Subscribed to {TOPIC_SUB}")
    else:
        print(f"❌ MQTT connect failed with code {rc}")

def on_message(client, userdata, msg):
    """
    Handle incoming button-count messages and publish A차 출발 if needed.
    """
    try:
        count = int(msg.payload.decode())
        print(f"📥 Received count: {count}")
        conn, cursor = userdata['db']
        update_load_count(cursor, conn, count)
        if count > 5:
            client.publish(TOPIC_PUB, "A차 출발", qos=1)
            print(f"🔄 Published 'A차 출발' to {TOPIC_PUB}")
    except ValueError:
        print("❌ Payload is not an integer.")

def create_mqtt_client(db_conn_tuple):
    """
    Create and return an MQTT client bound to our handlers.
    """
    client = mqtt.Client(userdata={'db': db_conn_tuple})
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, keepalive=60)
    return client

# ─── Main Execution: Sensor loop + DB + MQTT ────────────────────
if __name__ == "__main__":
    # 1) Sensor GPIO 초기화
    setup()

    # 2) DB 연결 & MQTT 클라이언트 시작
    conn, cursor    = connect_db()
    mqtt_client     = create_mqtt_client((conn, cursor))
    mqtt_client.loop_start()

    try:
        while True:
            # 3) 거리 측정 → DB 저장
            dist = measure_distance()
            print(f"🔍 Distance: {dist} cm")
            insert_distance(cursor, conn, dist)

            # 4) 거리 5cm 미만일 때 C차 출발 명령 발행
            if dist < 5:
                mqtt_client.publish(TOPIC_PUB_DIST, "C차 출발", qos=1)
                print(f"⚡ Published 'C차 출발' to {TOPIC_PUB_DIST}")

            time.sleep(1)

    except KeyboardInterrupt:
        # Ctrl+C → 정상 종료
        pass

    finally:
        # 5) 정리: MQTT, GPIO, DB 리소스 해제
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        cleanup()
        cursor.close()
        conn.close()
        print("✔️ Shutdown complete.")
