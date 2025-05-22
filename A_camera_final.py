import time
import threading
import sys

from flask import Flask, Response, render_template_string
from picamera2 import Picamera2
import numpy as np
import cv2

import paho.mqtt.client as mqtt

# ─── MQTT 설정 ───────────────────────────────────────────────────────
BROKER     = "broker.hivemq.com"
PORT       = 1883
TOPIC_OUT  = "myhome/piA/qr"        # QR 데이터를 보낼 토픽

# MQTT 콜백 정의
def on_connect(client, userdata, flags, rc):
    print(f"[MQTT] Connected with result code {rc}")

def on_publish(client, userdata, mid):
    print(f"[MQTT] Message {mid} published.")

# MQTT 클라이언트 초기화
mqtt_client = mqtt.Client("PiA_Camera")
mqtt_client.on_connect = on_connect
mqtt_client.on_publish = on_publish
mqtt_client.connect(BROKER, PORT, keepalive=60)
mqtt_client.loop_start()  # 백그라운드 스레드로 MQTT 루프 시작

# ─── 카메라 & Flask 설정 ────────────────────────────────────────────
class MyPiCamera():
    def __init__(self, width, height):
        self.cap = Picamera2()
        self.width = width
        self.height = height
        self.is_open = True
        try:
            cfg = self.cap.create_video_configuration(
                main={"format":"RGB888","size":(width,height)}
            )
            self.cap.align_configuration(cfg)
            self.cap.configure(cfg)
            self.cap.start()
        except Exception as e:
            print("Camera init failed:", e)
            self.is_open = False

    def read(self, dst=None):
        if dst is None:
            dst = np.empty((self.height, self.width, 3), dtype=np.uint8)
        if self.is_open:
            dst = self.cap.capture_array()
        return self.is_open, dst

    def isOpened(self):
        return self.is_open

    def release(self):
        if self.is_open:
            self.cap.close()
        self.is_open = False

app = Flask(__name__)
camera = MyPiCamera(640, 480)
qr_detector = cv2.QRCodeDetector()


def gen_frames():
    last_data = None
    while camera.isOpened():
        success, frame = camera.read()
        if not success:
            break

        # QR 코드 감지 및 디코딩
        data, bbox, _ = qr_detector.detectAndDecode(frame)
        if data:
            # 중복 전송 방지: 마지막과 다를 때만 publish
            if data != last_data:
                print(f"[QR] Detected: {data}")
                mqtt_client.publish(TOPIC_OUT, data)
                last_data = data
            else:
                print("동일한 QR입니다.")

            # # 박스 그리기
            if bbox is not None:
                bbox = bbox.astype(int)
                for i in range(len(bbox[0])):
                    pt1 = tuple(bbox[0][i])
                    pt2 = tuple(bbox[0][(i + 1) % len(bbox[0])])
                    cv2.line(frame, pt1, pt2, (0,255,0), 2)

        # JPEG 인코딩 & 스트리밍
        _, buffer = cv2.imencode('.jpg', frame)
        frame_bytes = buffer.tobytes()
        yield (
            b'--frame\r\n'
            b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n'
        )

@app.route('/')
def index():
    return render_template_string('''
        <html>
        <head><title>Pi Camera Stream + MQTT</title></head>
        <body>
            <h1>📷 라즈베리파이 카메라 스트리밍 & QR → MQTT 퍼블리시</h1>
            <img src="/video_feed" width="640" height="480">
            <p>감지된 QR 코드는 MQTT로 전송됩니다.</p>
        </body>
        </html>
    ''')

@app.route('/video_feed')
def video_feed():
    return Response(
        gen_frames(),
        mimetype='multipart/x-mixed-replace; boundary=frame'
    )

if __name__ == '__main__':
    try:
        app.run(host='0.0.0.0', port=5000, threaded=True)
    finally:
        camera.release()
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
