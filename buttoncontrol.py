import RPi.GPIO as GPIO
import time
import threading
import paho.mqtt.client as mqtt
from container_dest import send_arrival 

BROKER = "broker.hivemq.com"   # 퍼블릭 MQTT 브로커 주소
PORT   = 1883                  # 일반 MQTT 포트 번호
TOPIC_PUB  = "myhome/button/count" # 버튼 누름 정보를 보낼 토픽, A가 토픽에 정보를 보냄
TOPIC_SUB = "myhome/command"


# 컨베이너 벨트 관련 GPIO 설정
# IN3 = 17   # L298N IN1
# IN4 = 27   # L298N IN2
# GPIO.setup(IN3, GPIO.OUT)
# GPIO.setup(IN4, GPIO.OUT)

button_pin = 17 # 버튼 핀 번호
GPIO.setwarnings(False)
GPIO.setmode(GPIO.BCM)
GPIO.setup(button_pin, GPIO.IN, pull_up_down=GPIO.PUD_DOWN) # 버튼 기본 상태 LOW

def on_connect(client, userdata, flags, rc): # 브로커에 연결되었을때 한번만 호출
    if rc == 0:
        print("👉 Connected to MQTT Broker")
        client.subscribe(TOPIC_SUB, qos=1)
    else:
        print(f"❌ Connection failed with code {rc}")

def on_message(client, userdata, msg):
    global count
    command = msg.payload.decode() 
    print(f"📬 Received command from B: {command}")
    
    # 보관함에서 A차 출발이라는 명령어 수신 시
    if command == "A차 출발":
        count = 1 # count 초기화
        print("🔄 count reset to 0")
        threading.Timer(3.0, send_arrival).start()

client = mqtt.Client()  # MQTT 클라이언트 객체 생성
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, keepalive=60)  # 브로커에 연결 (60초 간격으로 살아있다는 신호 보냄)
client.loop_start()

count=0 # 버튼 누름 횟수 카운트
prev_input = GPIO.LOW # 이전상태저장

try:
    while True:
        if GPIO.input(button_pin) == GPIO.HIGH and prev_input == GPIO.LOW:
            # 여기에 버튼이 눌렸을 때 실행할 코드
            
            print("count: ", count)
            #payload = str(count) # MQTT 메시지 보낼 내용, 카운트 값을 문자열로 변환
            # if GPIO.input(button_pin) == GPIO.HIGH:
            #     # 버튼 눌림 → 정방향 회전
            #     GPIO.output(IN3, GPIO.HIGH)
            #     GPIO.output(IN4, GPIO.LOW)
            # else:
            #     # 버튼 안 눌림 → 정지
            #     GPIO.output(IN3, GPIO.LOW)
            #     GPIO.output(IN4, GPIO.LOW)
            # time.sleep(0.05)
            result = client.publish(TOPIC_PUB, str(count),qos=1) # 브로커에로 메시지 전송
            count += 1
            # 전송 결과 확인 (0이면 성공)
            if result[0] == 0:
                print(f"📤 [A] Published {count} to {TOPIC_PUB}")
            else:
                print("❌ [A] Publish failed")
            time.sleep(0.2)

        prev_input = GPIO.input(button_pin) # 현재상태저장
        time.sleep(0.01)
except KeyboardInterrupt:
    pass
finally:
    client.loop_stop() #루프 중지
    client.disconnect() # 브로커와 연결 해제
    GPIO.cleanup() # GPIO 핀 정리