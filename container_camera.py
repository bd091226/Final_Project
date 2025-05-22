import paho.mqtt.client as mqtt

# ─── MQTT 브로커 정보 ───────────────────────────────────────────
BROKER    = "broker.hivemq.com"
PORT      = 1883
TOPIC_IN  = "myhome/piA/qr"    # PiA에서 QR 데이터 보낼 때 사용한 토픽
TOPIC_ACK = "myhome/piB/ack"   # PiB에서 ACK 또는 처리 결과를 보낼 토픽

# ─── QR 데이터 처리 함수 ─────────────────────────────────────────
def process_qr_data(qr_data: str):
    """
    QR 코드로부터 받은 문자열을 처리합니다.
    예: '서울 과자' -> 지역과 상품으로 분리
    """
    parts = qr_data.split()
    if len(parts) >= 2:
        region, item = parts[0], parts[1]
        print(f"🔍 Parsed QR - Region: {region}, Item: {item}")
        # 필요한 추가 로직을 여기에서 호출하세요.
        # 예: 데이터베이스 저장, 카메라 촬영 트리거 등
        return region, item
    else:
        print("⚠️ QR 데이터 형식이 올바르지 않습니다. 공백으로 구분된 두 가지 값이 필요합니다.")
        return None, None

# ─── 콜백 정의 ──────────────────────────────────────────────────
def on_connect(client, userdata, flags, rc):
    print(f"[PiB] Connected with result code {rc}")
    client.subscribe(TOPIC_IN)
    print(f"[PiB] Subscribed to {TOPIC_IN}")

def on_message(client, userdata, msg):
    qr_data = msg.payload.decode().strip()
    print(f"[PiB] Received QR from PiA: {qr_data}")

    # 1) QR 데이터 파싱
    region, item = process_qr_data(qr_data)

    # 2) 필요 시 ACK 메시지 발행
    ack_payload = f"ACK: region={region}, item={item}" if region and item else "ACK: invalid format"
    client.publish(TOPIC_ACK, ack_payload)
    print(f"[PiB] Published ACK to {TOPIC_ACK}: {ack_payload}")

# ─── 클라이언트 설정 및 실행 ────────────────────────────────────
if __name__ == "__main__":
    client = mqtt.Client("PiB_QR_Receiver")
    client.on_connect = on_connect
    client.on_message = on_message

    client.connect(BROKER, PORT, keepalive=60)
    client.loop_forever()
