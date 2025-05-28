import time
import numpy as np
import cv2
from flask import Flask, Response, render_template_string
from picamera2 import Picamera2
from pyzbar.pyzbar import decode
import paho.mqtt.client as mqtt

# ─── MQTT 설정 ─────────────────────────────────────
BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC_OUT = "myhome/piA/qr"

def on_connect(client, userdata, flags, rc):
    print(f"[MQTT] Connected with result code {rc}")

def on_publish(client, userdata, mid):
    print(f"[MQTT] Message {mid} published.")

mqtt_client = mqtt.Client("PiA_Camera")
mqtt_client.on_connect = on_connect
mqtt_client.on_publish = on_publish
mqtt_client.connect(BROKER, PORT, keepalive=60)
mqtt_client.loop_start()

# ─── 카메라 설정 ───────────────────────────────────
class MyPiCamera():
    def __init__(self, width, height):
        self.cap = Picamera2()
        self.width = width
        self.height = height
        self.is_open = True
        try:
            cfg = self.cap.create_video_configuration(
                main={"format": "RGB888", "size": (width, height)}
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

# ─── Flask 앱 설정 ────────────────────────────────
app = Flask(__name__)
camera = MyPiCamera(640, 480)

def gen_frames():
    last_data = None
    while camera.isOpened():
        success, frame = camera.read()
        if not success:
            break

        # RGB to BGR
        frame_bgr = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
        gray = cv2.cvtColor(frame_bgr, cv2.COLOR_BGR2GRAY)

        qr_codes = decode(gray)
        for qr in qr_codes:
            data = qr.data.decode('utf-8')
            # if data != last_data:
            print(f"[QR] Detected: {data}")
            mqtt_client.publish(TOPIC_OUT, data)
            #     last_data = data
            # else:
            #     print("동일한 QR입니다.")

            # 바운딩 박스 그리기
            (x, y, w, h) = qr.rect
            cv2.rectangle(frame_bgr, (x, y), (x + w, y + h), (0, 255, 0), 2)

        # JPEG 인코딩
        _, buffer = cv2.imencode('.jpg', frame_bgr)
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
            <h1>📷 라즈베리파이 카메라 스트리밍 & QR → MQTT 퍼블리시 (pyzbar)</h1>
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
