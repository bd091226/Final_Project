import numpy as np
import random
import os
import matplotlib.pyplot as plt

# 맵 정의
raw_map = [
    "0000B000",
    "00000000",
    "00A0B000",
    "00000000",
    "S0000000",
    "00000000",
    "00C00000",
    "0000D000"
]

height = len(raw_map)
width = len(raw_map[0])
actions = [(-1, 0), (1, 0), (0, -1), (0, 1)]  # 상, 하, 좌, 우
action_size = len(actions)

map_matrix = []
start_pos = None
goals = {}

for y in range(height):
    row = []
    for x in range(width):
        ch = raw_map[y][x]
        row.append(ch)
        if ch == 'S':
            start_pos = (y, x)
        elif ch in ['A', 'B', 'C', 'D']:
            goals[ch] = (y, x)
    map_matrix.append(row)

goal_name = 'A'
goal_pos = goals[goal_name]

# Q-table 파일 이름
q_table_filename = "Q_table_A.npy"

# Q-table 초기화 또는 불러오기
if os.path.exists(q_table_filename):
    Q_table = np.load(q_table_filename)
    print(f"{q_table_filename} 불러오기 완료.")
else:
    Q_table = np.zeros((height, width, action_size))
    print("Q_table 새로 생성됨.")

# 파라미터
alpha = 0.1
gamma = 0.9
epsilon = 0.2
epsilon_min = 0.01
epsilon_decay = 0.995
max_episodes_per_run = 500  # while 한 번 돌 때마다 학습할 에피소드 수
max_total_episodes = 100000
max_steps = 200

total_episodes = 0
rewards_per_episode = []

print("Q-learning 학습 시작...")

while True:
    for _ in range(max_episodes_per_run):
        state = start_pos
        visited_goal = False
        total_reward = 0

        for _ in range(max_steps):
            y, x = state
            if random.random() < epsilon:
                action_index = random.randint(0, action_size - 1)
            else:
                action_index = np.argmax(Q_table[y][x])

            dy, dx = actions[action_index]
            ny, nx = y + dy, x + dx

            if 0 <= ny < height and 0 <= nx < width and map_matrix[ny][nx] != '1':
                reward = -1
                done = False
                if not visited_goal and (ny, nx) == goal_pos:
                    reward = 100
                    visited_goal = True
                    done = True  # 목표 도달 시 바로 종료하도록 수정

                Q_table[y][x][action_index] += alpha * (
                    reward + gamma * np.max(Q_table[ny][nx]) - Q_table[y][x][action_index]
                )
                state = (ny, nx)
                total_reward += reward
                if done:
                    break
            else:
                reward = -10
                Q_table[y][x][action_index] += alpha * (reward - Q_table[y][x][action_index])
                total_reward += reward

        rewards_per_episode.append(total_reward)
        total_episodes += 1

        # ✅ 추가: 보상이 100 이상이면 조기 종료
        if total_reward >= 100:
            print(f"목표 도달 보상 {total_reward} 획득! 에피소드 {total_episodes}에서 학습 종료합니다.")
            break

    # Q-table 저장
    np.save(q_table_filename, Q_table)
    print(f"{q_table_filename} 저장됨. 총 학습 에피소드: {total_episodes}")

    # 종료 조건 확인 (최근 100개 에피소드 표준편차)
    if len(rewards_per_episode) >= 100:
        recent_std = np.std(rewards_per_episode[-100:])
        if recent_std < 5:
            print(f"학습 안정됨! 총 에피소드 {total_episodes}에서 종료합니다.")
            break

    if total_episodes >= max_total_episodes:
        break

# 보상 그래프 (스무딩 적용)
def moving_average(data, window=100):
    return np.convolve(data, np.ones(window)/window, mode='valid')

plt.figure(figsize=(10, 4))
plt.plot(moving_average(rewards_per_episode), label="Smoothed")
plt.xlabel('Episode')
plt.ylabel('Total Reward')
plt.title('Reward per Episode (Smoothed)')
plt.grid(True)
plt.tight_layout()
plt.savefig('reward_per_episode_continued.png')
print("보상 그래프 저장 완료: reward_per_episode_continued.png")
