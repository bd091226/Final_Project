import RPi.GPIO as GPIO
import time
import paho.mqtt.client as mqtt

# ─── GPIO 핀 번호 설정 (BCM 모드) ──────────────────────────────────
GPIO.setmode(GPIO.BCM)
LED_PIN = 18    # LED 연결 핀 (예: BCM 18)
GPIO.setup(LED_PIN, GPIO.OUT)
GPIO.output(LED_PIN, GPIO.LOW)

# ─── MQTT 설정 ──────────────────────────────────────────────────
BROKER      = "broker.hivemq.com"
PORT        = 1883
TOPIC_ALERT = "myhome/distance/alert"

def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("👉 C: MQTT Broker에 연결되었습니다.")
        client.subscribe(TOPIC_ALERT, qos=1)
    else:
        print(f"❌ C: MQTT 연결 실패, 코드 {rc}")

def on_message(client, userdata, msg):
    payload = msg.payload.decode().strip()
    if payload == "출발":
        GPIO.output(LED_PIN, GPIO.HIGH)
        print("출발")
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
        GPIO.cleanup()
        print("✔️ C: GPIO 정리 완료")
