import pymysql
import paho.mqtt.client as mqtt
import time

# MySQL 접속 정보
DB_CONFIG = {
    "user": "Project_19",
    "password": "1234",
    "host": "192.168.137.148",
    "database": "Project_19",
    "charset": "utf8"
}

MQTT_BROKER = 'broker.hivemq.com'
MQTT_PORT = 1883
MQTT_TOPIC = 'vehicle/B/start'

def check_saturation_and_publish():
    try:
        conn = pymysql.connect(**DB_CONFIG)
        cursor = conn.cursor()
        cursor.execute("SELECT 구역_ID FROM 구역 WHERE 포화_여부 = 0")
        rows = cursor.fetchall()

        if rows:
            zone_ids = [str(r[0]) for r in rows]
            message = f"SATURATED_ZONES:{','.join(zone_ids)}"
            mqtt_client.publish(MQTT_TOPIC, payload=message)
            print(f"Published message: {message}")
        else:
            print(f"No saturated zones.")

    except Exception as e:
        print("MySQL 오류:", e)
    finally:
        cursor.close()
        conn.close()

if __name__ == '__main__':
    mqtt_client = mqtt.Client()
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)

    try:
        while True:
            check_saturation_and_publish()
            time.sleep(10)
    except KeyboardInterrupt:
        print("종료 중...")
    finally:
        mqtt_client.disconnect()
