# container_camera.py
import paho.mqtt.client as mqtt

# ─── MQTT 브로커 정보 ───────────────────────────────────────────
BROKER    = "broker.hivemq.com"
PORT      = 1883
TOPIC_IN  = "myhome/piA/qr"    # PiA에서 QR 데이터 보낼 때 사용한 토픽

# ─── QR 데이터 처리 함수 ─────────────────────────────────────────
def process_qr_data(qr_data: str):
    """
    QR 코드로부터 받은 문자열을 공백으로 분리하여
    첫 번째 값을 타입(type), 두 번째 값을 데이터(data)로 반환합니다.
    """
    parts = qr_data.strip().split()
    if len(parts) >= 2:
        type_, data = parts[0], parts[1]
        print(f"[PiB] Parsed QR - Type: {type_}, Data: {data}")
        return type_, data
    else:
        print("[PiB] Invalid QR format. Expected at least two values separated by space.")
        return None, None

# ─── 콜백 정의 ──────────────────────────────────────────────────
def on_connect(client, userdata, flags, rc):
    print(f"[PiB] Connected with result code {rc}")
    client.subscribe(TOPIC_IN)
    print(f"[PiB] Subscribed to {TOPIC_IN}")


def on_message(client, userdata, msg):
    qr_data = msg.payload.decode()
    print(f"[PiB] Received QR from PiA: {qr_data}")

    # QR 데이터 파싱
    type_, data = process_qr_data(qr_data)

    # 필요 시 ACK 메시지 발행 (옵션)
    # if type_ and data:
    #     ack_payload = f"ACK: type={type_}, data={data}"
    #     client.publish("myhome/piB/ack", ack_payload, qos=1)
    #     print(f"[PiB] Published ACK: {ack_payload}")

# ─── 클라이언트 설정 및 실행 ────────────────────────────────────
client = mqtt.Client("PiB_QR_Receiver")
client.on_connect = on_connect
client.on_message = on_message

client.connect(BROKER, PORT, keepalive=60)
client.loop_forever()
