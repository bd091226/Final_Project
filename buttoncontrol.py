import time
import threading
import RPi.GPIO as GPIO
import paho.mqtt.client as mqtt
import next_dest

# ─── 상수 정의 ───────────────────────────────────────
BROKER       = "broker.hivemq.com"
PORT         = 1883
TOPIC_PUB    = "myhome/button/count"
TOPIC_SUB    = "myhome/command"
DEBOUNCE_MS  = 50     # 버튼 디바운싱 (ms)

motor_lock = False
MOTOR_IN1    = 22
MOTOR_IN2    = 27
BUTTON_PIN   = 17

# ─── GPIO 초기화 ────────────────────────────────────
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)
GPIO.setup(MOTOR_IN1, GPIO.OUT)
GPIO.setup(MOTOR_IN2, GPIO.OUT)
GPIO.setup(BUTTON_PIN, GPIO.IN, pull_up_down=GPIO.PUD_UP)

# 서보관련 함수 정의
def motor_forward():
    GPIO.output(MOTOR_IN1, GPIO.HIGH)
    GPIO.output(MOTOR_IN2, GPIO.LOW)

def motor_stop():
    GPIO.output(MOTOR_IN1, GPIO.LOW)
    GPIO.output(MOTOR_IN2, GPIO.LOW)

def task():
    global motor_lock
    time.sleep(3)  # 3초 대기
    
    # destination = next_dest.current_destination
    
    print(f"A차 목적지 도착")
    client.publish("myhome/arrival", "A차 목적지 도착", qos=1)

    # 모터 2초 작동
    motor_lock = True
    GPIO.output(MOTOR_IN1, GPIO.HIGH)
    GPIO.output(MOTOR_IN2, GPIO.LOW)
    time.sleep(2)
    GPIO.output(MOTOR_IN1, GPIO.LOW)
    GPIO.output(MOTOR_IN2, GPIO.LOW)
    motor_lock = False


# ─── MQTT 콜백 정의 ─────────────────────────────────
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("👉 MQTT 연결 성공")
        client.subscribe(TOPIC_SUB, qos=1)
    else:
        print(f"❌ MQTT 연결 실패 (코드 {rc})")

def on_message(client, userdata, msg):
    global count
    payload = msg.payload.decode()
    print(f"📬 명령 수신: {payload}")
    if payload.startswith("A차가 ") and payload.endswith("로 출발"):
        # 메시지에서 목적지만 추출
        destination = payload[len("A차가 "):-len("로 출발")]
        # next_dest 모듈에도 동기화
        next_dest.current_destination = destination
        
        count = 1
        print("🔄 count 초기화")
        print(f"🚗 {destination}로 출발합니다!")
        threading.Thread(target=task).start()
        


# ─── MQTT 클라이언트 설정 ───────────────────────────
client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, keepalive=60)
client.loop_start()
# 목적지 수신기 시작 (next_dest.py의 MQTT listener 실행)
next_dest.destination_listen()

# ─── 메인 변수 ───────────────────────────────────────
count = 1
prev_state = GPIO.HIGH
arrival_mode =False

# ─── 버튼 상태 검사 및 MQTT 발행 ────────────────────
try:
    print("버튼을 누르고 있으면 모터가 계속 회전합니다.")
    while True:
        cur_state = GPIO.input(BUTTON_PIN)

        if cur_state == GPIO.LOW:
            # 버튼이 눌려 있는 동안 모터 구동
            if not motor_lock:
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
            if not motor_lock:
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
