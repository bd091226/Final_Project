import RPi.GPIO as GPIO
import time
import numpy as np

# 핀 설정 (L298N 기준)
IN1 = 17  # 왼쪽 모터 IN1
IN2 = 18  # 왼쪽 모터 IN2
IN3 = 22  # 오른쪽 모터 IN3
IN4 = 23  # 오른쪽 모터 IN4

GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)
GPIO.setup([IN1, IN2, IN3, IN4], GPIO.OUT)

def set_motors(left_dir, right_dir):
    # 왼쪽 모터 방향 제어
    if left_dir == 'forward':
        GPIO.output(IN1, GPIO.HIGH)
        GPIO.output(IN2, GPIO.LOW)
    elif left_dir == 'backward':
        GPIO.output(IN1, GPIO.LOW)
        GPIO.output(IN2, GPIO.HIGH)
    else:  # stop
        GPIO.output(IN1, GPIO.LOW)
        GPIO.output(IN2, GPIO.LOW)

    # 오른쪽 모터 방향 제어
    if right_dir == 'forward':
        GPIO.output(IN3, GPIO.HIGH)
        GPIO.output(IN4, GPIO.LOW)
    elif right_dir == 'backward':
        GPIO.output(IN3, GPIO.LOW)
        GPIO.output(IN4, GPIO.HIGH)
    else:  # stop
        GPIO.output(IN3, GPIO.LOW)
        GPIO.output(IN4, GPIO.LOW)

def stop():
    set_motors('stop', 'stop')

def forward(duration=0.6):
    set_motors('forward', 'forward')
    time.sleep(duration)
    stop()

def backward(duration=0.6):
    set_motors('backward', 'backward')
    time.sleep(duration)
    stop()

def left(duration=0.5):
    # 좌회전: 왼쪽 후진, 오른쪽 전진
    set_motors('backward', 'forward')
    time.sleep(duration)
    stop()

def right(duration=0.5):
    # 우회전: 왼쪽 전진, 오른쪽 후진
    set_motors('forward', 'backward')
    time.sleep(duration)
    stop()

# Q-table 액션 및 함수 매핑
actions = [(-1, 0), (1, 0), (0, -1), (0, 1)]  # 상, 하, 좌, 우
move_funcs = [forward, backward, left, right]

# Q-table 불러오기
q_table_filename = "Q_table_A.npy"
Q_table = np.load(q_table_filename, allow_pickle=True)

height, width, _ = Q_table.shape

start_pos = (4, 0)
goal_pos = (2, 2)
current_pos = start_pos
visited = set()

try:
    for _ in range(50):
        y, x = current_pos
        action_index = int(np.argmax(Q_table[y][x]))
        dy, dx = actions[action_index]
        ny, nx = y + dy, x + dx

        if (ny, nx) in visited or not (0 <= ny < height and 0 <= nx < width):
            print(f"잘못된 경로로 종료합니다: {current_pos} -> {(ny, nx)}")
            break

        print(f"Move: {current_pos} -> {(ny, nx)}")
        print(f"Q[{y}][{x}] = {Q_table[y][x]} -> action {action_index}")

        move_funcs[action_index]()  # PWM 없이 단순 동작, duration 기본값 사용
        current_pos = (ny, nx)
        visited.add(current_pos)

        if current_pos == goal_pos:
            print("목적지에 도착했습니다!")
            break

finally:
    stop()
    GPIO.cleanup()
