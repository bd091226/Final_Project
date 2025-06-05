# motor_control.py

import RPi.GPIO as GPIO
import time
import numpy as np
from path_planner import *

# ------------------------ GPIO Motor Setup ------------------------

IN1 = 17
IN2 = 18
ENA = 12
IN3 = 22
IN4 = 23
ENB = 13
PWM_FREQ = 1000

DIRECTIONS = ['N', 'E', 'S', 'W']
DIR_VECTORS = {'N': (-1, 0), 'E': (0, 1), 'S': (1, 0), 'W': (0, -1)}

def get_turn_action(current_dir, target_vec):
    dir_idx = DIRECTIONS.index(current_dir)
    for i in range(4):
        candidate_dir = DIRECTIONS[(dir_idx + i) % 4]
        if DIR_VECTORS[candidate_dir] == target_vec:
            if i == 0:
                return 'F', candidate_dir
            elif i == 1:
                return 'R', candidate_dir
            elif i == 2:
                return 'U', candidate_dir
            elif i == 3:
                return 'L', candidate_dir
    return None, current_dir

def setup():
    GPIO.setmode(GPIO.BCM)
    GPIO.setwarnings(False)
    GPIO.setup(IN1, GPIO.OUT)
    GPIO.setup(IN2, GPIO.OUT)
    GPIO.setup(ENA, GPIO.OUT)
    GPIO.setup(IN3, GPIO.OUT)
    GPIO.setup(IN4, GPIO.OUT)
    GPIO.setup(ENB, GPIO.OUT)
    global pwmA, pwmB
    pwmA = GPIO.PWM(ENA, PWM_FREQ)
    pwmB = GPIO.PWM(ENB, PWM_FREQ)
    pwmA.start(0)
    pwmB.start(0)

def set_speed(speedA, speedB):
    pwmA.ChangeDutyCycle(speedA)
    pwmB.ChangeDutyCycle(speedB)

def forward(speed=60):
    GPIO.output(IN1, GPIO.HIGH)
    GPIO.output(IN2, GPIO.LOW)
    GPIO.output(IN3, GPIO.HIGH)
    GPIO.output(IN4, GPIO.LOW)
    set_speed(speed, speed)

def backward(speed=60):
    GPIO.output(IN1, GPIO.LOW)
    GPIO.output(IN2, GPIO.HIGH)
    GPIO.output(IN3, GPIO.LOW)
    GPIO.output(IN4, GPIO.HIGH)
    set_speed(speed, speed)

def turn_left(speed=50):
    GPIO.output(IN1, GPIO.LOW)
    GPIO.output(IN2, GPIO.HIGH)
    GPIO.output(IN3, GPIO.HIGH)
    GPIO.output(IN4, GPIO.LOW)
    set_speed(speed, speed)

def turn_right(speed=50):
    GPIO.output(IN1, GPIO.HIGH)
    GPIO.output(IN2, GPIO.LOW)
    GPIO.output(IN3, GPIO.LOW)
    GPIO.output(IN4, GPIO.HIGH)
    set_speed(speed, speed)

def stop():
    GPIO.output(IN1, GPIO.LOW)
    GPIO.output(IN2, GPIO.LOW)
    GPIO.output(IN3, GPIO.LOW)
    GPIO.output(IN4, GPIO.LOW)
    set_speed(0, 0)

def cleanup():
    pwmA.stop()
    pwmB.stop()
    GPIO.cleanup()

def move_step(curr, next_, step_count, path, start, goal, current_dir):
    dx, dy = next_[0] - curr[0], next_[1] - curr[1]
    move_vec = (dx, dy)
    action, new_dir = get_turn_action(current_dir, move_vec)

    if action == 'F':
        print("⬆️ 직진")
        forward()
    elif action == 'L':
        print("⬅️ 좌회전")
        turn_left()
        time.sleep(3.4)
        forward()
    elif action == 'R':
        print("➡️ 우회전")
        turn_right()
        time.sleep(3.4)
        forward()
    elif action == 'U':
        print("🔄 유턴")
        turn_right()
        time.sleep(1.7)
        turn_right()
        time.sleep(1.7)
        forward()
    else:
        print("⚠️ 방향 계산 실패:", dx, dy)
        stop()

    save_plot(path, next_, start, goal, step_count)
    print(f"위치 시각화 저장: position_step_{step_count}.png")
    time.sleep(0.7)
    stop()
    time.sleep(0.3)

    return new_dir

def follow_path(path, start, goal, initial_dir):
    current_dir = initial_dir
    save_plot(path, path[0], start, goal, 0)
    print("위치 시각화 저장: position_step_0.png")
    for i in range(len(path)-1):
        current_dir = move_step(path[i], path[i+1], i+1, path, start, goal, current_dir)

def main():
    try:
        setup()
        S = find_position('S')
        A = find_position('A')
        B = find_position('B')
        C = find_position('C')
        D = find_position('D')

        goals = [A, B, C, D]
        goal_names = ['A', 'B', 'C', 'D']

        Q_tables = []
        rewards_list = []

        start = S

        for goal, name in zip(goals, goal_names):
            q_filename = f"q_S_to_{name}.npy"
            try:
                Q_init = np.load(q_filename)
                print(f"기존 Q_{start}to{name} 불러옴")
            except FileNotFoundError:
                Q_init = None
                print(f"새로운 Q_{start}to{name} 학습 시작")

            Q, rewards = q_learning(start, goal, Q_init=Q_init, episodes=10000)
            np.save(q_filename, Q)
            Q_tables.append(Q)
            rewards_list.append(rewards)
            print(f"Q_{start}to{name} 저장 완료")

        plot_rewards(rewards_list, [f'S → {n}' for n in goal_names])
        print("모든 목적지에 대한 보상 그래프 저장 완료")

        # A → S 학습 추가
        q_filename_return = f"q_A_to_S.npy"
        try:
            Q_return = np.load(q_filename_return)
            print(f"기존 Q_AtoS 불러옴")
        except FileNotFoundError:
            Q_return = None
            print(f"새로운 Q_AtoS 학습 시작")

        Q_return, rewards_return = q_learning(A, S, Q_init=Q_return, episodes=10000)
        np.save(q_filename_return, Q_return)
        print("Q_AtoS 저장 완료")

        # 예시로 A 목적지 경로 따라 이동
        initial_dir = 'E'
        print(f"\n[차량 이동 시작: S → A]")
        path_A = extract_path(Q_tables[0], start, A)
        print("경로:", path_A)
        follow_path(path_A, start, A, initial_dir)
        # 그리고 A → S 이동
        print(f"\n[차량 이동 시작: A → S]")
        path_return = extract_path(Q_return, A, S)
        print("경로:", path_return)
        follow_path(path_return, A, S, initial_dir)

    finally:
        stop()
        cleanup()

if __name__ == "__main__":
    main()
