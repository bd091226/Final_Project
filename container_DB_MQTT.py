# 구역함 DB와 MQTT 연결 (초음파 센서, A차 수신 적재 수량 업데이트 가능)
import RPi.GPIO as GPIO
import time
import mysql.connector
from mysql.connector import Error
import paho.mqtt.client as mqtt
import subprocess
import os
import sys

# ─── A_car_camer.py 자동 실행 ────────────────────────────────────
# 현재 스크립트 디렉토리 기준으로 상대경로 지정
SCRIPT_DIR = os.path.dirname(os.path.realpath(__file__))
CAMERA_SCRIPT = os.path.join(SCRIPT_DIR, 'A_car_camer.py')

# 자식 프로세스로 A_car_camer.py 실행
camera_proc = subprocess.Popen([sys.executable, CAMERA_SCRIPT])
print(f"▶️ Started A_car_camer.py (PID {camera_proc.pid})")

# ─── GPIO 핀 번호 설정 (BCM 모드) ──────────────────────────────────
GPIO.setmode(GPIO.BCM)
TRIG_PIN = 23  # Trigger 핀 (BCM 23)
ECHO_PIN = 24  # Echo 핀    (BCM 24)
GPIO.setup(TRIG_PIN, GPIO.OUT)
GPIO.setup(ECHO_PIN, GPIO.IN)

# ─── DB 연결 정보 ────────────────────────────────────────────────
DB_CONFIG = {
    "user": "Project_19",
    "password": "1234",
    "host": "192.168.137.148",
    "database": "Project_19",
    "charset": "utf8"
}

# ─── MQTT 설정 ──────────────────────────────────────────────────
BROKER    = "broker.hivemq.com"
PORT      = 1883
TOPIC_SUB = "myhome/button/count"
TOPIC_PUB = "myhome/command"

# ─── 전역 상태 변수 ──────────────────────────────────────────────
count = 0
conn = None
cursor = None

def connect_db():
    global conn, cursor
    while True:
        try:
            conn = mysql.connector.connect(**DB_CONFIG)
            if conn.is_connected():
                cursor = conn.cursor()
                print(f"▶️ MySQL 서버에 연결되었습니다 (DB: {DB_CONFIG['database']}).")
                return
        except Error as e:
            print(f"❌ MySQL 연결 실패: {e}. 5초 후 재시도합니다...")
            time.sleep(5)

def measure_distance():
    GPIO.output(TRIG_PIN, False)
    time.sleep(0.5)
    GPIO.output(TRIG_PIN, True)
    time.sleep(0.00001)
    GPIO.output(TRIG_PIN, False)
    while GPIO.input(ECHO_PIN) == 0:
        pulse_start = time.time()
    while GPIO.input(ECHO_PIN) == 1:
        pulse_end = time.time()
    return round((pulse_end - pulse_start) * 17150, 2)

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("👉 Connected to MQTT Broker")
        client.subscribe(TOPIC_SUB, qos=1)
    else:
        print(f"❌ MQTT 연결 실패, 코드 {rc}")

def on_message(client, userdata, msg):
    global count, conn, cursor
    try:
        count_value = int(msg.payload.decode())
        print(f"📥 Received count: {count_value} (topic: {msg.topic})")
        count = count_value

        try:
            cursor.execute(
                "UPDATE vehicle_status_A SET load_count = %s WHERE vehicle_id = 1",
                (count,)
            )
            conn.commit()
            print(f"🔄 vehicle_status_A.load_count updated to {count}")
        except Error as db_err:
            print(f"❌ 차량 현황 업데이트 오류: {db_err}")
            conn.close()
            connect_db()

        if count > 5:
            command = "A차 출발"
            client.publish(TOPIC_PUB, command, qos=1)
            print(f"🔄 Published command to {TOPIC_PUB}: {command}")
            count = 0

    except ValueError:
        print("❌ 수신한 payload가 정수가 아닙니다")

def main():
    connect_db()

    mqtt_client = mqtt.Client(client_id="B_Subscriber")
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message
    mqtt_client.connect(BROKER, PORT, keepalive=60)
    mqtt_client.loop_start()

    try:
        while True:
            dist = measure_distance()
            print(f"🔍 측정된 거리: {dist} cm")
            try:
                cursor.execute(
                    "INSERT INTO `z_Seoul` (distance) VALUES (%s)",
                    (dist,)
                )
                conn.commit()
            except Error as e:
                print(f"❌ DB 삽입 오류: {e}")
                conn.close()
                connect_db()
            time.sleep(1)

    except KeyboardInterrupt:
        print("\n🚪 프로그램 종료 중...")

    finally:
        # MQTT 및 GPIO 정리
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        GPIO.cleanup()

        # 자식 프로세스 종료
        if camera_proc.poll() is None:
            camera_proc.terminate()
            print(f"✔️ A_car_camer.py (PID {camera_proc.pid}) 종료함")

        # DB 리소스 정리
        try:
            cursor.close()
            conn.close()
        except:
            pass

        print("✔️ 종료 및 리소스 정리 완료")

if __name__ == "__main__":
    main()
