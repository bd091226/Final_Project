import RPi.GPIO as GPIO
import time
import paho.mqtt.client as mqtt

# ─── GPIO 핀 번호 설정 (BCM 모드) ──────────────────────────────────
GPIO.setmode(GPIO.BCM)
LED_PIN = 18    # LED 연결 핀 (예: BCM 18)
SERVO_PIN = 16    # 서보모터 연결 핀
GPIO.setup(LED_PIN, GPIO.OUT)
GPIO.output(LED_PIN, GPIO.LOW)

# 서보 GPIO 설정
GPIO.setmode(GPIO.BCM)
GPIO.setup(SERVO_PIN, GPIO.OUT)
pwm = GPIO.PWM(SERVO_PIN, 50)
pwm.start(0)

def set_angle(angle):
    # 각도를 듀티사이클로 변환 (SG90 기준)
    duty = 2 + (angle / 18)
    pwm.ChangeDutyCycle(duty)
    time.sleep(0.5)
    pwm.ChangeDutyCycle(0)

# ─── MQTT 설정 ──────────────────────────────────────────────────
BROKER      = "broker.hivemq.com"
PORT        = 1883
TOPIC_ALERT = "myhome/distance/alert"
TOPIC_STATUS     = "myhome/distance/status"  # C → B 발행 토픽

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("👉 C: MQTT Broker에 연결되었습니다.")
        client.subscribe(TOPIC_ALERT, qos=1)
    else:
        print(f"❌ C: MQTT 연결 실패, 코드 {rc}")

def on_message(client, userdata, msg):
    payload = msg.payload.decode().strip()
    if payload == "B차 출발":
        GPIO.output(LED_PIN, GPIO.HIGH)
        print("출발")
        # B에게 '목적지 도착' 메시지 발행
        client.publish(TOPIC_STATUS, "목적지 도착", qos=1)
        print("📨 보관함 → B: '목적지 도착' 전송")

        set_angle(0)
        time.sleep(1)

        set_angle(90)
        time.sleep(1)
        print("서보모터: 90도 회전")

        # 모터에 안정 시간을 주고 PWM 0으로 설정
        time.sleep(0.5)

    else:
        GPIO.output(LED_PIN, GPIO.LOW)
        print(f"알 수 없는 명령: {payload}")

def main():
    mqtt_client = mqtt.Client(client_id="C_Subscriber")
    mqtt_client.on_connect = on_connect
    mqtt_client.on_message = on_message

    mqtt_client.connect(BROKER, PORT, keepalive=60)
    mqtt_client.loop_forever()

if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        print("\n🚪 C 프로그램 종료 중…")
    finally:
        pwm.stop()
        GPIO.cleanup()
        print("✔️ C: GPIO 정리 완료")
