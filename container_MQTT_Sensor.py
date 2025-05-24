# container_MQTT_Sensor.py

import paho.mqtt.client as mqtt
import RPi.GPIO as GPIO
import time
from container_config import (
    BROKER, PORT,
    TOPIC_SUB,
    TOPIC_PUB,
    TOPIC_PUB_DIST,
    TOPIC_STATUS,
    TOPIC_ARRIVAL
)

from container_DB import button_A, zone_arrival_A, transfer_stock_zone_to_vehicle, departed_A

# --- 핀 설정 ---
TRIG_PIN = 23
ECHO_PIN = 24
SERVO_PIN = 12
gpio_initialized = False  # 플래그로 중복 초기화 방지

# --- GPIO 초기화 ---
def initialize_gpio():
    global gpio_initialized
    if not gpio_initialized:
        GPIO.setmode(GPIO.BCM)
        GPIO.setup(TRIG_PIN, GPIO.OUT)
        GPIO.setup(ECHO_PIN, GPIO.IN)
        GPIO.setup(SERVO_PIN, GPIO.OUT)
        gpio_initialized = True

# --- 초음파 센서 ---
def measure_distance():
    GPIO.output(TRIG_PIN, False)
    time.sleep(0.05)
    GPIO.output(TRIG_PIN, True)
    time.sleep(0.00001)
    GPIO.output(TRIG_PIN, False)

    pulse_start = time.time()
    pulse_end = time.time()

    timeout = time.time() + 0.04
    while GPIO.input(ECHO_PIN) == 0 and time.time() < timeout:
        pulse_start = time.time()

    timeout = time.time() + 0.04
    while GPIO.input(ECHO_PIN) == 1 and time.time() < timeout:
        pulse_end = time.time()

    pulse_duration = pulse_end - pulse_start
    distance = round(pulse_duration * 17150, 2)
    return distance

# --- 서보모터 ---
def setup_servo():
    pwm = GPIO.PWM(SERVO_PIN, 50)
    pwm.start(0)
    return pwm

def move_servo(pwm, angle):
    duty = 2 + (angle / 18.0)
    print(f"[서보] 각도: {angle}°, 듀티: {duty:.2f}%")
    pwm.ChangeDutyCycle(duty)
    time.sleep(0.7)
    pwm.ChangeDutyCycle(0)

# --- MQTT 콜백 ---
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        client.subscribe(TOPIC_SUB, qos=1)
        client.subscribe(TOPIC_PUB_DIST, qos=1)
        client.subscribe(TOPIC_STATUS, qos=1)
        client.subscribe(TOPIC_ARRIVAL, qos=1)
        print("✅ MQTT 연결 및 구독 완료")
    else:
        print(f"❌ MQTT 연결 실패: 코드 {rc}")

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode().strip()
    conn, cursor, pwm = userdata['db_pwm']

    if topic == TOPIC_SUB:
        try:
            count = int(payload)
            button_A(cursor, conn, count)
            if count > 2:
                client.publish(TOPIC_PUB, "A차 출발", qos=1)
                print(f"🚗 A차 출발 메시지 발행 → {TOPIC_PUB}")
                departed_A(conn, cursor, vehicle_id=1)
                
        except ValueError:
            print("❌ 잘못된 숫자 payload")

    elif topic == TOPIC_PUB_DIST:
        print(f"📥 B차 거리 조건 충족 메시지 수신: '{payload}'")

    elif topic == TOPIC_STATUS:
        print(f"📥 B차 상태 메시지 수신: '{payload}'")
        if payload == "B차 목적지 도착":
            print("🎯 B차가 목적지에 도착했습니다! 서보모터를 90°로 회전합니다.")
            move_servo(pwm, 90)
            time.sleep(0.5)
            move_servo(pwm, 0)
            transfer_stock_zone_to_vehicle(conn, cursor)
            
    elif topic == TOPIC_ARRIVAL: 
        print(f"📥 도착 메시지 수신: '{payload}'")
        if payload == "A차 목적지 도착":
            print("🎯 A가 목적지에 도착")
            zone_arrival_A(conn, cursor)

# --- 센서 루프 ---
def run_sensor_loop(mqtt_client, conn, cursor):
    try:
        while True:
            dist = measure_distance()
            print(f"📏 거리 측정: {dist} cm")

            if dist < 5:
                mqtt_client.publish(TOPIC_PUB_DIST, "B차 출발", qos=1)
                print(f"🚗 MQTT 발행: 'B차 출발' → {TOPIC_PUB_DIST}")

            time.sleep(1)
    except KeyboardInterrupt:
        pass  # 메인에서 종료 처리

# --- MQTT 클라이언트 생성 ---
def create_mqtt_client_with_servo(db_conn_tuple):
    pwm = setup_servo()
    client = mqtt.Client(userdata={'db_pwm': (*db_conn_tuple, pwm)})
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, keepalive=60)
    return client, pwm

# --- 직접 실행 시 ---
if __name__ == "__main__":
    from container_DB import connect_db
    initialize_gpio()
    conn, cursor = connect_db()
    client, pwm = create_mqtt_client_with_servo((conn, cursor))

    try:
        client.loop_start()
        run_sensor_loop(client, conn, cursor)
    finally:
        client.loop_stop()
        pwm.stop()
        cursor.close()
        conn.close()
        GPIO.cleanup()
        print("🛑 시스템 종료 완료")
