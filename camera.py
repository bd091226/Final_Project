# 파일명: aruco_stream_picamera2_full.py

import cv2 # opencv라이브러리 불러옴
import numpy as np # 수치 연산
import threading
from flask import Flask, Response, render_template_string
from picamera2 import Picamera2
import time

# html 구현
HTML_PAGE = """
<!doctype html>
<html>
  <head>
    <title>Raspberry Pi ArUco Streaming + Pose</title>
    <style>
      body { margin: 0; padding: 0; background: #f5f5f5; }
      .container { text-align: center; margin-top: 10px; }
      h2 { font-family: Arial, sans-serif; color: #333; }
      img { 
        border: 1px solid #ccc; 
        max-width: 90%; 
        height: auto; 
      }
    </style>
  </head>
  <body>
    <div class="container">
      <h2>ArUco Marker Detection + Pose Estimation (Streaming)</h2>
      <img src="{{ url_for('video_feed') }}" />
    </div>
  </body>
</html>
"""

# Flask 앱
app = Flask(__name__)

# Picamera2 카메라 초기화
try:
    picam2 = Picamera2() # 카메라 컨트롤 객체를 생성
    # Preview configuration: BGR888 포맷으로 받아옴
    config = picam2.create_preview_configuration(
        main={"format": "BGR888", "size": (640, 480)}
    ) # 영상 스트리망을 위한 설정
    picam2.configure(config) # 실제 카메라 객체에 적용
    picam2.start() # 카메라 스트리밍 시작
    # 2초 대기
    time.sleep(2)
except Exception as e:
    print(f"[ERROR] 카메라 초기화 실패: {e}")
    exit(1)

# 카메라 내부 파라미터 설정 (Pose Estimation용)
# 아래 fx, fy, ppx, ppy 값은 예시(Pixel 단위)입니다.
# 실제 사용하는 카메라의 교정(calibration) 결과값으로 바꿔주세요.
fx, fy = 615.0, 615.0       # 초점 거리 (픽셀)
ppx, ppy = 320.0, 240.0     # 광학 중심 (이미지 중심)
camera_matrix = np.array(
    [[fx,  0, ppx],
     [ 0, fy, ppy],
     [ 0,  0,   1]], dtype=np.float32
) # 카메라 내부 파라미터 행렬 -> numpy배열로 생성
dist_coeffs = np.zeros((5, 1), dtype=np.float32)  # 왜곡 계수 (없으면 0으로)

# ArUco 마커 딕셔너리 & 파라미터 설정
# 6x6, 250개 짜리 딕셔너리 사용 (필요에 맞게 변경)
aruco_dict = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_6X6_250)

# OpenCV 버전에 따라 DetectorParameters 생성 방식이 달라질 수 있음
try:
    aruco_params = cv2.aruco.DetectorParameters_create()
except AttributeError:
    aruco_params = cv2.aruco.DetectorParameters()

# 마커 실제 길이(mm 단위) — 사용 중인 마커 크기로 설정
marker_length = 35.0

# 전역 변수: 스레드에서 그린 프레임을 저장하기 위한 버퍼
frame_lock = threading.Lock()
output_frame = None

# ArUco 검출 + Pose Estimation 스레드 함수
def detect_and_estimate():
    global output_frame

    while True:
        frame = picam2.capture_array() # 실시간 BGR이미지를 가져옴
        if frame is None:
            # 프레임이 비어있는 경우 잠시 대기하고 재시도
            time.sleep(0.01)
            continue

        # 그레이스케일로 변환
        gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)

        # 마커 검출
        corners, ids, rejected = cv2.aruco.detectMarkers(
            gray, aruco_dict, parameters=aruco_params
        )

        if ids is not None and len(ids) > 0:
            # 감지된 마커 그리기
            cv2.aruco.drawDetectedMarkers(frame, corners, ids)

            # 4) 감지된 각 마커에 대해 Pose 추정
            rvecs, tvecs, _ = cv2.aruco.estimatePoseSingleMarkers(
                corners, marker_length, camera_matrix, dist_coeffs
            )

            for i in range(len(ids)):
                # 좌표축(Axis) 그리기 (축 길이 = marker_length/2)
                # cv2.aruco.drawAxis(
                #     frame, camera_matrix, dist_coeffs,
                #     rvecs[i], tvecs[i], marker_length / 2
                # )

                # 위치(tvec)와 회전(rvec)을 텍스트로 표시
                tvec = tvecs[i][0]
                rvec = rvecs[i][0]
                x, y, z = tvec
                rx, ry, rz = np.rad2deg(rvec)

                # 마커 중심에 텍스트를 가깝게 표시할 좌표 계산
                corner_points = corners[i][0]  # (4,2) 형태
                text_x = int(corner_points[:, 0].mean())
                text_y = int(corner_points[:, 1].mean())

                # ID 텍스트
                cv2.putText(
                    frame, f"ID:{ids[i][0]}",
                    (text_x - 20, text_y - 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2
                )
                # 위치 텍스트 (mm 단위)
                cv2.putText(
                    frame, f"Pos: {x:.1f},{y:.1f},{z:.1f} mm",
                    (text_x - 20, text_y),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 2
                )
                # 회전 텍스트 (도 단위)
                cv2.putText(
                    frame, f"Rot: {rx:.1f},{ry:.1f},{rz:.1f} deg",
                    (text_x - 20, text_y + 20),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 0, 0), 2
                )

        # 5) 스레드 안전하게 전역 버퍼에 복사
        with frame_lock:
            output_frame = frame.copy()

        # 약간의 딜레이로 빠른 루프 방지
        time.sleep(0.03)

# ---------------------------------------------------
# 8) MJPEG 스트리밍 제너레이터
# ---------------------------------------------------
def generate_mjpeg():
    global output_frame

    while True:
        with frame_lock:
            if output_frame is None:
                # 아직 프레임이 준비되지 않으면 잠시 대기
                time.sleep(0.01)
                continue

            # JPEG 인코딩
            ret, buffer = cv2.imencode('.jpg', output_frame)
            if not ret:
                # 인코딩 실패 시 다시 반복
                time.sleep(0.01)
                continue
            frame_bytes = buffer.tobytes()

        # MJPEG 바운더리 형식으로 보내기
        yield (b'--frame\r\n'
               b'Content-Type: image/jpeg\r\n\r\n' +
               frame_bytes + b'\r\n')

# Flask 라우팅 정의
@app.route('/')
def index():
    # HTML 템플릿 렌더링
    return render_template_string(HTML_PAGE)

@app.route('/video_feed')
def video_feed():
    # MJPEG 스트리밍 엔드포인트
    return Response(
        generate_mjpeg(),
        mimetype='multipart/x-mixed-replace; boundary=frame'
    )

if __name__ == '__main__':
    # ArUco 인식 + 포즈 추정 스레드 띄우기
    t = threading.Thread(target=detect_and_estimate, daemon=True)
    t.start()

    # Flask 웹 서버 실행
    # host='0.0.0.0' → LAN 내 다른 기기에서도 접근 가능
    # port=5000 → URL: http://<RPI_IP>:5000
    app.run(host='0.0.0.0', port=5000, threaded=True)
