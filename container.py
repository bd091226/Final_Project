import paho.mqtt.client as mqtt

# MQTT 설정 (A 쪽과 동일하게)
BROKER = "broker.hivemq.com"
PORT   = 1883
TOPIC_SUB  = "myhome/button/count" # A가 B에게 보내는 주제
TOPIC_PUB = "myhome/command" # B가 A에게 보내는 주제

count = 0
# 연결 성공 콜백
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("👉 Connected to MQTT Broker")
        client.subscribe(TOPIC_SUB, qos=1)
    else:
        print(f"❌ Connection failed with code {rc}")

# 메시지 수신 콜백
def on_message(client, userdata, msg):
    global count

    try:
        count_value = int(msg.payload.decode())
        print(f"📥 Received count: {count_value} (topic: {msg.topic})")
        count = count_value

        if count > 5:
            command = "A차 출발"
            result = client.publish(TOPIC_PUB, command, qos=1)
            # if result[0] == 0:
            #     print(f"📤 [B] Published command to {TOPIC_PUB}: {command}")
            # else:
            #     print("❌ [B] Command publish failed")

            count = 0  # B 측 count 초기화
            print("🔄 [B] count reset to 0 after sending command")

    except ValueError:
        print("❌ Received payload is not an integer")

# 클라이언트 생성 및 콜백 등록
client = mqtt.Client(client_id="B_Subscriber")
client.on_connect = on_connect
client.on_message  = on_message

# 브로커에 연결 및 루프 진입
client.connect(BROKER, PORT, keepalive=60)
client.loop_forever()