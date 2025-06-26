from picamera2 import Picamera2
import cv2
import numpy as np
import paho.mqtt.client as mqtt
import json
import time

# MQTT 설정
mqtt_client = mqtt.Client()
mqtt_client.connect("broker.hivemq.com",1883, 60)
mqtt_client.loop_start()

# 카메라 설정c
picam2 = Picamera2()
picam2.preview_configuration.main.size = (640, 480)
picam2.preview_configuration.main.format = "BGR888"
picam2.configure("preview")
picam2.start()
time.sleep(1)  # 카메라 워밍업 시간

# ArUco 설정
aruco_dict = cv2.aruco.getPredefinedDictionary(cv2.aruco.DICT_6X6_250)
parameters = cv2.aruco.DetectorParameters()

# 카메라 내부 파라미터 (예시 값)
camera_matrix = np.array([[600, 0, 320],
                          [0, 600, 240],
                          [0, 0, 1]], dtype=np.float32)
dist_coeffs = np.zeros((5, 1))

print("📷 ArUco 마커 감지 및 MQTT 전송 시작")

while True:
    frame = picam2.capture_array()
    gray = cv2.cvtColor(frame, cv2.COLOR_RGB2GRAY)
    corners, ids, _ = cv2.aruco.detectMarkers(gray, aruco_dict, parameters=parameters)

    if ids is not None:
        rvecs, tvecs, _ = cv2.aruco.estimatePoseSingleMarkers(corners, 0.05, camera_matrix, dist_coeffs)
        for i in range(len(ids)):
            x_cm = tvecs[i][0][0] * 100
            y_cm = tvecs[i][0][1] * 100
            z_cm = tvecs[i][0][2] * 100

            # 방향 정보 (Yaw 추출)
            rvec = rvecs[i][0]
            yaw = float(rvec[1])  # radians
            data = {
                "id": int(ids[i][0]),
                "x": round(x_cm, 2),
                "y": round(y_cm, 2),
                "z": round(z_cm, 2),
                "yaw": round(yaw, 4)
            }
            # print(f"📡 전송: {data}")
            mqtt_client.publish("storage/gr_B", json.dumps(data))

    time.sleep(0.5)  # 10Hz로 전송 (필요 시 조절)