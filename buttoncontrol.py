import RPi.GPIO as GPIO
import time
import paho.mqtt.client as mqtt

BROKER = "broker.hivemq.com"   # 퍼블릭 MQTT 브로커 주소
PORT   = 1883                  # 일반 MQTT 포트 번호
TOPIC_PUB  = "myhome/button/count" # 버튼 누름 정보를 보낼 토픽, A가 토픽에 정보를 보냄
TOPIC_SUB = "myhome/command"  

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
    if command == "A차 출발":
        count = 1 # count 초기화
        print("🔄 count reset to 0")

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