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
TOPIC_A_NEXT = 'vehicle/A/next'

def check_saturation_and_publish():
    conn= None
    cursor= None
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
        if cursor:
            cursor.close()
        if conn:
            conn.close()

def A_next_dest(operation_id):

    conn = pymysql.connect(**DB_CONFIG)
    cursor = conn.cursor()
    try:
        sql = """
            SELECT 구역_ID
            FROM 운행_상품
            WHERE 운행_ID = %s
              AND A차운송_시각 IS NOT NULL
              AND 투입_시각 IS NULL
            ORDER BY 적재_순번 ASC
            LIMIT 1
        """
        cursor.execute(sql, (operation_id,))
        row = cursor.fetchone()
        if row:
            next_zone = row[0]
            msg = f"A차가 {next_zone}로 출발"
            print(f"[A-next] 운행_ID={operation_id}의 다음 구역: {next_zone}")
            mqtt_client.publish(TOPIC_A_NEXT, msg, qos=1)
            print(f"[A-next] Published: {msg}")
        else:
            print(f"[A-next] 운행_ID={operation_id}에 남은 구역이 없습니다.")
    except Exception as e:
        print("MySQL 오류 (A-next):", e)
    finally:
        cursor.close()
        conn.close()

if __name__ == '__main__':
    mqtt_client = mqtt.Client()
    mqtt_client.connect(MQTT_BROKER, MQTT_PORT, 60)
    operation_id=100
    try:
        while True:
            check_saturation_and_publish()
            A_next_dest(operation_id)
            time.sleep(10)
    except KeyboardInterrupt:
        print("종료 중...")
    finally:
        mqtt_client.disconnect()
