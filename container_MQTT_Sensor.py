# container_MQTT.py

import paho.mqtt.client as mqtt
import RPi.GPIO as GPIO
import time
from container_config import (
    BROKER, PORT,
    TOPIC_SUB,        # 버튼 카운트
    TOPIC_PUB,        # A차 출발
    TOPIC_PUB_DIST,   # B차 출발 알림 (구독도 함)
    TOPIC_STATUS
)
from container_DB import update_load_count, insert_distance, handle_qr_insert

# --- 핀 설정 ---
TRIG_PIN = 23
ECHO_PIN = 24
SERVO_PIN = 12

GPIO.setmode(GPIO.BCM)

# --- 초음파 센서 함수 ---
def setup_ultrasonic():
    GPIO.setup(TRIG_PIN, GPIO.OUT)
    GPIO.setup(ECHO_PIN, GPIO.IN)

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

def cleanup_ultrasonic():
    GPIO.cleanup([TRIG_PIN, ECHO_PIN])

# --- 서보모터 함수 ---
def setup_servo():
    GPIO.setup(SERVO_PIN, GPIO.OUT)
    pwm = GPIO.PWM(SERVO_PIN, 50)
    return pwm

def move_servo(pwm, angle):
    duty = 2 + (angle / 18)
    print(f"[서보] 각도: {angle}°, 듀티: {round(duty, 2)}%")
    pwm.ChangeDutyCycle(duty)
    time.sleep(0.7)
    pwm.ChangeDutyCycle(0)

def cleanup_servo(pwm):
    pwm.stop()
    GPIO.cleanup([SERVO_PIN])

# --- MQTT 콜백 함수 ---
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
        print(f"📥 B차 측 거리 조건 충족 메시지 수신: '{payload}'")

    elif topic == TOPIC_STATUS:
        print(f"📥 B차 상태 메시지 수신: '{payload}'")

        if payload == "목적지 도착":
            print("🎯 B차가 목적지에 도착했습니다! 서보 작동 시작")
            try:
                pwm = setup_servo()
                move_servo(pwm, 180)
                time.sleep(1.5)
                cleanup_servo(pwm)
                print("🛠️ 서보모터 180도 위치로 회전 완료")
            except Exception as e:
                print(f"❌ 서보모터 동작 중 오류: {e}")

# --- 센서 루프 실행 ---
def run_sensor_loop(mqtt_client, conn, cursor):
    setup_ultrasonic()
    try:
        while True:
            dist = measure_distance()
            print(f"📏 거리 측정: {dist} cm")

            insert_distance(cursor, conn, dist)

            if dist < 5:
                mqtt_client.publish(TOPIC_PUB_DIST, "B차 출발", qos=1)
                print(f"🚗 MQTT 발행: 'B차 출발' → {TOPIC_PUB_DIST}")

            time.sleep(1)
    finally:
        cleanup_ultrasonic()

# --- MQTT 클라이언트 생성 ---
def create_mqtt_client(db_conn_tuple):
    client = mqtt.Client(userdata={'db': db_conn_tuple})
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, keepalive=60)
    return client
