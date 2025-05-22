import paho.mqtt.client as mqtt

# ─── MQTT 브로커 정보 ───────────────────────────────────────────
BROKER    = "broker.hivemq.com"
PORT      = 1883
TOPIC_IN  = "myhome/piA/qr"    # PiA에서 QR 데이터 보낼 때 사용한 토픽

# ─── 콜백 정의 ──────────────────────────────────────────────────
def on_connect(client, userdata, flags, rc):
    print(f"[PiB] Connected with result code {rc}")
    client.subscribe(TOPIC_IN)
    print(f"[PiB] Subscribed to {TOPIC_IN}")

def on_message(client, userdata, msg):
    qr_data = msg.payload.decode()
    print(f"[PiB] Received QR from PiA: {qr_data}")
    # 만약 PiB에서 응답(ACK) 메시지를 보내고 싶다면 여기에 publish 추가
    # client.publish("myhome/piB/ack", f"Got QR: {qr_data}")

# ─── 클라이언트 설정 및 실행 ────────────────────────────────────
client = mqtt.Client("PiB_QR_Receiver")
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, keepalive=60)
client.loop_forever()
