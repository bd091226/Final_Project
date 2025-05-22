from flask import Flask, Response, render_template_string
from picamera2 import Picamera2
import numpy as np
import cv2
import sys

class MyPiCamera():
    def __init__(self, width, height):
        self.cap = Picamera2()
        self.width = width
        self.height = height
        self.is_open = True

        try:
            self.config = self.cap.create_video_configuration(main={"format":"RGB888","size":(width,height)})
            self.cap.align_configuration(self.config)
            self.cap.configure(self.config)
            self.cap.start()
        except:
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

# Flask 웹 서버 구성
app = Flask(__name__)
camera = MyPiCamera(640, 480)
# QR 코드 감지를 위한 디텍터
qr_detector = cv2.QRCodeDetector()

# 영상 프레임 생성 (JPEG로 변환)
def gen_frames():
    while camera.isOpened():
        success, frame = camera.read()
        if not success:
            break
        
         # QR 코드 감지 및 디코딩
        data, bbox, _ = qr_detector.detectAndDecode(frame)
        if data:
            print("Detected QR code:")
            print(data)
            sys.stdout.flush()
            # 바운딩 박스 그리기 (선택 사항)
            if bbox is not None:
                bbox = bbox.astype(int)
                for i in range(len(bbox[0])):
                    pt1 = tuple(bbox[0][i])
                    pt2 = tuple(bbox[0][(i + 1) % len(bbox[0])])
                    cv2.line(frame, pt1, pt2, (0, 255, 0), 2)

        # JPEG로 인코딩
        _, buffer = cv2.imencode('.jpg', frame)
        frame_bytes = buffer.tobytes()
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + frame_bytes + b'\r\n')


# 메인 페이지 라우트
@app.route('/')
def index():
    return render_template_string('''
        <html>
        <head><title>Pi Camera Stream</title></head>
        <body>
            <h1>📷 라즈베리파이 카메라 실시간 스트리밍</h1>
            <img src="/video_feed" width="640" height="480">
        </body>
        </html>
    ''')

# 비디오 피드 라우트
@app.route('/video_feed')
def video_feed():
    return Response(gen_frames(), mimetype='multipart/x-mixed-replace; boundary=frame')

# 서버 실행
if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000)
