# DB_publisher.py
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
TOPIC_A_CURRENT_DEST = 'A_current_dest'

def check_saturation_and_publish():
    conn= None
    cursor= None
    try:
        conn = pymysql.connect(**DB_CONFIG)
        cursor = conn.cursor()
        cursor.execute("SELECT 구역_ID FROM 구역 WHERE 포화_여부 = 1")
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

def A_current_dest(mqtt_client,operation_id): # A에게 보낼 목적지를 데이터베이스에서 들고오는 함수
    #operation_id는 지금은 고정되어 있지만 나중에 교체되어야할 것
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
            current_zone = row[0]
            msg = f"A차 {current_zone}로 출발" 
            print(f"[A-current] 운행_ID={operation_id}의 현재 목적지: {current_zone}") 
            mqtt_client.publish(TOPIC_A_CURRENT_DEST, msg, qos=1) # A에게 현재 목적지 발행
            print(f"[A-current] Published: {msg}")
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
            A_current_dest(operation_id)
            time.sleep(10)
    except KeyboardInterrupt:
        print("종료 중...")
    finally:
        mqtt_client.disconnect()
