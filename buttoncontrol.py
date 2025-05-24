import time
import threading
import RPi.GPIO as GPIO
import paho.mqtt.client as mqtt
from container_dest import send_arrival

# ─── 상수 정의 ───────────────────────────────────────
BROKER       = "broker.hivemq.com"
PORT         = 1883
TOPIC_PUB    = "myhome/button/count"
TOPIC_SUB    = "myhome/command"
DEBOUNCE_MS  = 50     # 버튼 디바운싱 (ms)

MOTOR_IN1    = 22
MOTOR_IN2    = 27
BUTTON_PIN   = 17

# ─── GPIO 초기화 ────────────────────────────────────
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)
GPIO.setup(MOTOR_IN1, GPIO.OUT)
GPIO.setup(MOTOR_IN2, GPIO.OUT)
GPIO.setup(BUTTON_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

# ─── MQTT 콜백 정의 ─────────────────────────────────
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("👉 MQTT 연결 성공")
        client.subscribe(TOPIC_SUB, qos=1)
    else:
        print(f"❌ MQTT 연결 실패 (코드 {rc})")

def on_message(client, userdata, msg):
    global count
    cmd = msg.payload.decode()
    print(f"📬 명령 수신: {cmd}")
    if cmd == "A차 출발":
        count = 1
        print("🔄 count 초기화")
        threading.Timer(3.0, send_arrival).start()

# ─── MQTT 클라이언트 설정 ───────────────────────────
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, keepalive=60)
client.loop_start()

# ─── 메인 변수 ───────────────────────────────────────
count = 1
prev_state = GPIO.HIGH

# ─── 모터 제어 헬퍼 ──────────────────────────────────
def motor_forward():
    GPIO.output(MOTOR_IN1, GPIO.HIGH)
    GPIO.output(MOTOR_IN2, GPIO.LOW)

def motor_stop():
    GPIO.output(MOTOR_IN1, GPIO.LOW)
    GPIO.output(MOTOR_IN2, GPIO.LOW)

# ─── 버튼 상태 검사 및 MQTT 발행 ────────────────────
try:
    print("버튼을 누르고 있으면 모터가 계속 회전합니다.")
    while True:
        cur_state = GPIO.input(BUTTON_PIN)

        if cur_state == GPIO.LOW:
            # 버튼이 눌려 있는 동안 모터 구동
            motor_forward()

            # ↑ edge (HIGH→LOW) 일 때만 count 발행
            if prev_state == GPIO.HIGH:
                print(f"count: {count}")
                result = client.publish(TOPIC_PUB, str(count), qos=1)
                if result[0] == 0:
                    print(f"📤 발행 성공 → {count}")
                else:
                    print("❌ 발행 실패")
                count += 1

            # 간단 디바운싱
            time.sleep(DEBOUNCE_MS / 1000.0)

        else:
            # 버튼을 떼면 모터 정지
            motor_stop()

        prev_state = cur_state
        time.sleep(0.01)

except KeyboardInterrupt:
    pass

finally:
    client.loop_stop()
    client.disconnect()
    GPIO.cleanup()
    print("종료 및 GPIO 클린업 완료")
