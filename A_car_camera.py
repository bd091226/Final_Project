import time
import sys
from flask import Flask, Response, render_template_string
from picamera2 import Picamera2
import numpy as np
import cv2
import paho.mqtt.client as mqtt
from pyzbar.pyzbar import decode

# ─── MQTT 설정 ────────────────────────────────────────────────
BROKER     = "broker.hivemq.com"
PORT       = 1883
TOPIC_OUT  = "myhome/piA/qr"  # B에게 보낼 토픽

mqtt_client = mqtt.Client("PiA_QR_Sender")
mqtt_client.connect(BROKER, PORT, 60)
mqtt_client.loop_start()

# ─── 카메라 설정 ──────────────────────────────────────────────
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

    def read(self):
        if self.is_open:
            return True, self.cap.capture_array()
        return False, None

    def release(self):
        if self.is_open:
            self.cap.close()
            self.is_open = False

# ─── Flask + Streaming ───────────────────────────────────────
app = Flask(__name__)
camera = MyPiCamera(640, 480)

def gen_frames():
    while camera.is_open:
        success, frame = camera.read()
        if not success:
            break

        decoded_objs = decode(frame)
        for obj in decoded_objs:
            data = obj.data.decode('utf-8')
            print(f"[QR] Detected: {data}")
            mqtt_client.publish(TOPIC_OUT, data)

            # QR 코드 테두리 그리기
            points = obj.polygon
            if len(points) > 1:
                for i in range(len(points)):
                    pt1 = tuple(points[i])
                    pt2 = tuple(points[(i + 1) % len(points)])
                    cv2.line(frame, pt1, pt2, (0, 255, 0), 2)

        _, buffer = cv2.imencode('.jpg', frame)
        frame_bytes = buffer.tobytes()
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')

@app.route('/')
def index():
    return render_template_string('''
        <html>
        <head><title>📷 Pi Camera + MQTT</title></head>
        <body>
            <h2>실시간 스트리밍 (QR 코드 감지 시 MQTT로 전송)</h2>
            <img src="/video_feed" width="640" height="480">
        </body>
        </html>
    ''')

@app.route('/video_feed')
def video_feed():
    return Response(gen_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

if __name__ == '__main__':
    try:
        app.run(host='0.0.0.0', port=5000, threaded=True)
    finally:
        camera.release()
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
