from picamera2 import Picamera2
import cv2
import numpy as np
from flask import Flask, Response
import time

app = Flask(__name__)

# 카메라 설정
picam2 = Picamera2()
picam2.preview_configuration.main.size = (640, 480)
picam2.preview_configuration.main.format = "BGR888"
picam2.configure("preview")
picam2.start()
time.sleep(1)  # 카메라 워밍업 시간

# ArUco 설정
aruco_dict = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_6X6_250)
parameters = cv2.aruco.DetectorParameters()

# 카메라 내부 파라미터 (예시 값, 정확한 값으로 교체 권장)
camera_matrix = np.array([[600, 0, 320],
                          [0, 600, 240],
                          [0, 0, 1]], dtype=np.float32)
dist_coeffs = np.zeros((5, 1))

def generate_frames():
    while True:
        frame = picam2.capture_array()
        gray = cv2.cvtColor(frame, cv2.COLOR_RGB2GRAY)

        corners, ids, _ = cv2.aruco.detectMarkers(gray, aruco_dict, parameters=parameters)

        if ids is not None:
            cv2.aruco.drawDetectedMarkers(frame, corners, ids)

            # 각 마커에 대해 pose estimation 수행
            rvecs, tvecs, _ = cv2.aruco.estimatePoseSingleMarkers(corners, 0.05, camera_matrix, dist_coeffs)

            for i in range(len(ids)):
                # 각 마커에 좌표축 그리기
                cv2.drawFrameAxes(frame, camera_matrix, dist_coeffs, rvecs[i], tvecs[i], 0.03)

                # 마커 중심에 ID 표시
                c = corners[i][0]
                center_x = int(np.mean(c[:, 0]))
                center_y = int(np.mean(c[:, 1]))
                cv2.putText(frame, f"ID:{ids[i][0]}", (center_x, center_y),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 255), 2)
                # 각 축 방향 거리 추출 (단위: meter → cm 변환)
                x_cm = tvecs[i][0][0] * 100
                y_cm = tvecs[i][0][1] * 100
                z_cm = tvecs[i][0][2] * 100

                # 전체 거리도 계산 (유클리드 거리)
                distance_cm = np.linalg.norm(tvecs[i]) * 100

                # 콘솔 출력
                print(f"ID: {ids[i][0]}, X: {x_cm:.2f}cm, Y: {y_cm:.2f}cm, Z: {z_cm:.2f}cm, 거리: {distance_cm:.2f}cm")

                # 프레임에 거리ㅔㅔ 정보 표시
                cv2.putText(frame, f"X:{x_cm:.1f} Y:{y_cm:.1f} Z:{z_cm:.1f} cm", (center_x, center_y + 20),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 0), 1)
                #cv2.putText(frame, f"Total: {distance_cm:.1f} cm", (center_x, center_y + 40),cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 0), 1)

        frame_bgr = cv2.cvtColor(frame, cv2.COLOR_RGB2BGR)
        ret, jpeg = cv2.imencode('.jpg', frame_bgr)
        if not ret:
            continue

        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' + jpeg.tobytes() + b'\r\n')

@app.route('/video_feed')
def video_feed():
    return Response(generate_frames(),
                    mimetype='multipart/x-mixed-replace; boundary=frame')

@app.route('/')
def index():
    return """
    <html>
      <head><title>ArUco Pose Stream</title></head>
      <body>
        <h1>Pi Camera ArUco 스트리밍 + Pose</h1>
        <img src="/video_feed" width="640" height="480" />
      </body>
    </html>
    """

if __name__ == '__main__':
    print("✅ http://<라즈베리파이_IP>:5000 에 접속하세요")
    app.run(host='0.0.0.0', port=5000)
