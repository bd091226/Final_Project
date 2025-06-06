import numpy as np
import random
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import os

# ------------------------ Map & Q-learning ------------------------

raw_map = [
    "00A1B",
    "11010",
    "S0000",
    "11010",
    "00C1D"
]

ROWS, COLS = len(raw_map), len(raw_map[0])
actions = [(-1, 0), (1, 0), (0, -1), (0, 1)]  # 상, 하, 좌, 우

def is_valid(x, y):
    return 0 <= x < ROWS and 0 <= y < COLS and raw_map[x][y] != '1'

def find_position(symbol):
    for i in range(ROWS):
        for j in range(COLS):
            if raw_map[i][j] == symbol:
                return (i, j)
    return None

def state_to_index(state):
    return state[0] * COLS + state[1]

def q_learning(start, goal, Q_init=None, episodes=5000, alpha=0.3, gamma=0.9):
    n_states = ROWS * COLS
    n_actions = len(actions)
    Q = np.zeros((n_states, n_actions)) if Q_init is None else Q_init.copy()
    episode_rewards = []

    epsilon_start = 0.5
    epsilon_min = 0.01
    epsilon_decay = 0.995
    epsilon = epsilon_start
    max_steps = 100

    for ep in range(episodes):
        state = start
        total_reward = 0
        steps = 0
        while state != goal and steps < max_steps:
            s_idx = state_to_index(state)
            if random.random() < epsilon:
                a_idx = random.randint(0, n_actions - 1)
            else:
                a_idx = np.argmax(Q[s_idx])

            dx, dy = actions[a_idx]
            nx, ny = state[0] + dx, state[1] + dy

            if not is_valid(nx, ny):
                next_state = state
                reward = -2  # 무효 이동 패널티 완화
            else:
                next_state = (nx, ny)
                reward = 10 if next_state == goal else -0.1  # 이동 페널티도 완화

            ns_idx = state_to_index(next_state)
            Q[s_idx][a_idx] += alpha * (reward + gamma * np.max(Q[ns_idx]) - Q[s_idx][a_idx])

            state = next_state
            total_reward += reward
            steps += 1

        episode_rewards.append(total_reward)
        epsilon = max(epsilon_min, epsilon * epsilon_decay)

    return Q, episode_rewards

def extract_path(Q, start, goal):
    path = [start]
    state = start
    visited = set()
    for _ in range(50):
        s_idx = state_to_index(state)
        a_idx = np.argmax(Q[s_idx])
        dx, dy = actions[a_idx]
        next_state = (state[0] + dx, state[1] + dy)
        if not is_valid(*next_state) or next_state in visited:
            break
        path.append(next_state)
        if next_state == goal:
            break
        visited.add(next_state)
        state = next_state
    return path

def save_path(path, filename):
    with open(filename, "w") as f:
        for x, y in path:
            f.write(f"{x} {y}\n")
    print(f"경로 저장 완료: {filename}")

def save_q_table(Q, filename="q_table.npy"):
    np.save(filename, Q)
    print(f"Q-table 저장됨: {filename}")

def load_q_table(filename="q_table.npy"):
    if os.path.exists(filename):
        print(f"Q-table 로드: {filename}")
        return np.load(filename)
    else:
        print(f"{filename} 없음. 새로 초기화함.")
        return None

def plot_rewards(rewards, filename):
    plt.figure()
    plt.plot(rewards)
    plt.xlabel("Episode")
    plt.ylabel("Total Reward")
    plt.title("Episode Reward Over Time")
    plt.grid()
    plt.savefig(filename)
    plt.close()
    print(f"보상 그래프 저장됨: {filename}")

# ------------------------ 메인 실행부 ------------------------

if __name__ == "__main__":
    while True:
        start_symbol = 'S'
        goal_symbols = ['A', 'B', 'C', 'D']

        start_pos = find_position(start_symbol)
        if start_pos is None:
            raise ValueError(f"시작 심볼 '{start_symbol}' 위치를 찾을 수 없습니다.")
 
        Q_table = load_q_table()  # 이전 Q-table 불러오기

        # 1. S -> 각 목표 학습
        for goal_symbol in goal_symbols:
            goal_pos = find_position(goal_symbol)
            if goal_pos is None:
                print(f"목표 심볼 '{goal_symbol}' 위치를 찾을 수 없습니다. 건너뜀.")
                continue

            q_filename = f"q_table_{start_symbol}_to_{goal_symbol}.npy"
            Q_table = load_q_table(q_filename)

            print(f"학습 시작: {start_symbol} → {goal_symbol}")
            Q_table, rewards = q_learning(start_pos, goal_pos, Q_init=Q_table)
            path = extract_path(Q_table, start_pos, goal_pos)

            path_filename = f"path_{start_symbol}_to_{goal_symbol}.txt"
            save_path(path, path_filename)

            reward_plot_filename = f"reward_{start_symbol}_to_{goal_symbol}.png"
            plot_rewards(rewards, reward_plot_filename)

            save_q_table(Q_table, q_filename)

        # 역방향 학습도 같은 방식으로
        for goal_symbol in goal_symbols:
            goal_pos = find_position(goal_symbol)
            if goal_pos is None:
                continue

            q_filename = f"q_table_{goal_symbol}_to_{start_symbol}.npy"
            Q_table = load_q_table(q_filename)

            print(f"역방향 학습 시작: {goal_symbol} → {start_symbol}")
            Q_table, rewards = q_learning(goal_pos, start_pos, Q_init=Q_table)
            path = extract_path(Q_table, goal_pos, start_pos)

            path_filename = f"path_{goal_symbol}_to_{start_symbol}.txt"
            save_path(path, path_filename)

            reward_plot_filename = f"reward_{goal_symbol}_to_{start_symbol}.png"
            plot_rewards(rewards, reward_plot_filename)

            save_q_table(Q_table, q_filename)

        print("모든 목적지에 대한 학습 및 경로 저장 완료")
