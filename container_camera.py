# container_camera.py

import paho.mqtt.client as mqtt
from container_DB import handle_qr_insert

# ─── MQTT 브로커 정보 ───────────────────────────────────────────
BROKER   = "broker.hivemq.com"
PORT     = 1883
TOPIC_IN = "myhome/piA/qr"

def on_connect(client, userdata, flags, rc):
    print(f"[PiB] Connected with result code {rc}")
    client.subscribe(TOPIC_IN)
    print(f"[PiB] Subscribed to {TOPIC_IN}")

def on_message(client, userdata, msg):
    qr_data = msg.payload.decode().strip()
    print(f"[PiB] Received QR from PiA: {qr_data}")

    # 공백 기준 분리: type_, data
    parts = qr_data.split(maxsplit=1)
    if len(parts) == 2:
        type_, data = parts
        print(f"[PiB] Parsed QR - Type: {type_}, Data: {data}")
        # DB에 반영
        handle_qr_insert(type_, data)
    else:
        print("[PiB] Invalid QR format; expected '<지역> <상품>'")

if __name__ == "__main__":
    client = mqtt.Client("PiB_QR_Receiver")
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, keepalive=60)
    client.loop_forever()
