import subprocess
import time
import os

BASE_DIR = "/home/pi/FinalProject/Final_Project"
camera_path = os.path.join(BASE_DIR, "camera_final.py")
button_path = os.path.join(BASE_DIR, "buttoncontrol.py")
# dest_path = os.path.join(BASE_DIR, "container_dest.py") -> 3초후 동작하는 "목적지 도착"이라는 문구를 보내는 것이면 없어도 되지만 일단 넣어줌
# camera_streamer.py와 button_apublisher.py를 동시에 실행
camera_proc = subprocess.Popen(['python3', camera_path])
button_proc = subprocess.Popen(['python3', button_path])
#dest_proc = subprocess.Popen(['python3', dest_path])


try:
    print("실행 중...")
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    print("\n 종료 요청 받음. 프로세스 종료 중...")
    camera_proc.terminate()
    button_proc.terminate()
    # dest_proc.terminate()
    camera_proc.wait()
    button_proc.wait()
    # dest_proc.wait()
    print("종료")
