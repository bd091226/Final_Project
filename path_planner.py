# path_planner.py

import numpy as np
import random
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

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

def state_to_index(state):
    return state[0] * COLS + state[1]

def q_learning(start, goal, episodes=10000, alpha=0.1, gamma=0.9, epsilon=0.2, Q_init=None):
    n_states = ROWS * COLS
    n_actions = len(actions)
    if Q_init is None:
        Q = np.zeros((n_states, n_actions))
    else:
        Q = Q_init
    episode_rewards = []  # 각 에피소드 총 보상 저장용

    for ep in range(episodes):
        state = start
        total_reward = 0
        while state != goal:
            s_idx = state_to_index(state)
            if random.random() < epsilon:
                a_idx = random.randint(0, n_actions - 1)
            else:
                a_idx = np.argmax(Q[s_idx])

            dx, dy = actions[a_idx]
            nx, ny = state[0] + dx, state[1] + dy

            if not is_valid(nx, ny):
                next_state = state
                reward = -5
            else:
                next_state = (nx, ny)
                reward = 10 if next_state == goal else -1

            ns_idx = state_to_index(next_state)
            Q[s_idx][a_idx] += alpha * (reward + gamma * np.max(Q[ns_idx]) - Q[s_idx][a_idx])
            state = next_state
            total_reward += reward
        episode_rewards.append(total_reward)

    return Q, episode_rewards

def plot_rewards(reward_lists, labels):
    """
    reward_lists: 리스트 안에 각 학습시 보상 리스트들이 들어감 [[ep_rewards_1], [ep_rewards_2], ...]
    labels: 각 보상 리스트에 해당하는 이름 리스트
    """
    plt.figure(figsize=(10,6))
    for rewards, label in zip(reward_lists, labels):
        # 보상 값이 너무 불규칙할 수 있으니 이동평균으로 부드럽게 표시
        window = 50
        smoothed = np.convolve(rewards, np.ones(window)/window, mode='valid')
        plt.plot(smoothed, label=label)
    plt.xlabel('Episode')
    plt.ylabel('Total Reward per Episode (Moving Average)')
    plt.title('Q-learning Reward Comparison')
    plt.legend()
    plt.grid(True)
    plt.savefig("reward_comparison.png")
    plt.close()
    print("보상 비교 그래프 저장됨: reward_comparison.png")


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

def save_plot(path, curr_pos, start, goal, step_num):
    map_data = np.array([[1 if c == '1' else 0 for c in row] for row in raw_map])
    fig, ax = plt.subplots(figsize=(6,6))
    ax.imshow(map_data, cmap='Greys', origin='upper')
    ax.scatter(start[1], start[0], marker='o', color='green', s=200, label='Start (S)')
    ax.scatter(goal[1], goal[0], marker='X', color='blue', s=200, label='Goal (A)')

    if len(path) > 1:
        xs = [p[1] for p in path]
        ys = [p[0] for p in path]
        ax.plot(xs, ys, linestyle='-', linewidth=3, color='orange', label='Path')

    ax.scatter(curr_pos[1], curr_pos[0], color='red', s=300, label='Current Position')
    ax.set_xticks(range(COLS))
    ax.set_yticks(range(ROWS))
    ax.set_xticklabels(range(COLS))
    ax.set_yticklabels(range(ROWS))
    ax.grid(True, color='lightgray', linestyle='--')
    ax.set_title(f"Step {step_num}: Current Position {curr_pos}")
    ax.legend(loc='upper right')
    plt.savefig(f"position_step_{step_num}.png")
    plt.close(fig)


