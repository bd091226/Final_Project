import heapq
import time
import sys

# 1사분면 기준 격자 지도
grid = [
    ['B', 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 1, 1, 1, 0, 1, 1, 1, 0],
    [0, 1, 1, 1, 0, 1, 1, 1, 0],
    [0, 1, 'K', 1, 0, 1, 'W', 1, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 0],
    [0, 1, 1, 1, 0, 1, 1, 1, 0],
    [0, 1, 1, 1, 0, 1, 1, 1, 0],
    [0, 1, 'S', 1, 0, 1, 'G', 1, 0],
    [0, 0, 0, 0, 0, 0, 0, 0, 'A']
]

ROWS = len(grid)
COLS = len(grid[0])

def find_positions(grid, targets):
    positions = {}
    for y in range(ROWS):
        for x in range(COLS):
            cell = grid[y][x]
            if cell in targets:
                positions[cell] = (x, ROWS - 1 - y)
    return positions

def heuristic(a, b):
    return abs(a[0] - b[0]) + abs(a[1] - b[1])

def astar_time_reserved(start, goal, grid, reserved):
    open_set = []
    heapq.heappush(open_set, (0, 0, start))
    came_from = {}
    g_score = {(start, 0): 0}

    while open_set:
        _, t, current = heapq.heappop(open_set)
        if current == goal:
            path = [(current, t)]
            while (current, t) in came_from:
                current, t = came_from[(current, t)]
                path.append((current, t))
            return path[::-1]

        x, y = current
        for dx, dy in [(-1,0), (1,0), (0,-1), (0,1), (0,0)]:
            if abs(dx) > 1 or abs(dy) > 1:
                continue
            nx, ny = x + dx, y + dy
            if 0 <= nx < COLS and 0 <= ny < ROWS:
                ry = ROWS - 1 - ny
                if grid[ry][nx] == 0 or isinstance(grid[ry][nx], str):
                    neighbor = (nx, ny)
                    if (neighbor, t+1) in reserved:
                        continue
                    tentative_g = g_score[(current, t)] + 1
                    if (neighbor, t+1) not in g_score or tentative_g < g_score[(neighbor, t+1)]:
                        came_from[(neighbor, t+1)] = (current, t)
                        g_score[(neighbor, t+1)] = tentative_g
                        f_score = tentative_g + heuristic(neighbor, goal)
                        heapq.heappush(open_set, (f_score, t+1, neighbor))
    return []

def build_reserved(path):
    return {(pos, t) for pos, t in path}

def flatten_path_with_stops(path, stop_positions, stay_time):
    flat = []
    last_pos = path[0][0]
    idx = 0
    while idx < len(path):
        pos, t = path[idx]
        flat.append(pos)
        if pos in stop_positions:
            flat.extend([pos] * (stay_time - 1))  # 총 stay_time초 포함
        idx += 1
    return flat

def simulate_paths(path_A, path_B, goals_A, goals_B, start_A, start_B, final_A, final_B):
    print(f"{'초':<5}{'A 위치':<12}{'B 위치':<12}{'A 상태':<12}{'B 상태':<12}")
    i = 0
    while True:
        pos_A = path_A[i] if i < len(path_A) else path_A[-1]
        pos_B = path_B[i] if i < len(path_B) else path_B[-1]

        def get_status(pos, path, goals, start, final, tick):
            if tick == 0 and pos == start:
                return "출발"
            if tick >= len(path) - 1 and pos == final:
                return "도착"
            if pos == final and tick < len(path) - 1:
                return "이동"
            for g in goals:
                if positions[g] == pos:
                    return f"{g} 상하차"
            return "이동"

        state_A = get_status(pos_A, path_A, goals_A, start_A, final_A, i)
        state_B = get_status(pos_B, path_B, goals_B, start_B, final_B, i)

        print(f"{i:<5}{str(pos_A):<12}{str(pos_B):<12}{state_A:<12}{state_B:<12}")
        sys.stdout.flush()
        time.sleep(1)
        i += 1

        if state_A == "도착" and state_B == "도착":
            break

# 경로 순서
positions = find_positions(grid, {'A', 'B', 'S', 'G', 'K', 'W'})
a_order = ['A', 'S', 'G', 'K', 'W', 'A']
b_order = ['B', 'K', 'W', 'G', 'S', 'B']

print("A차 경로:", ' → '.join(a_order))
print("B차 경로:", ' → '.join(b_order))

# A차 경로
reserved_A = set()
path_A = []
current = positions[a_order[0]]
for target in a_order[1:]:
    segment = astar_time_reserved(current, positions[target], grid, reserved_A)
    reserved_A.update(build_reserved(segment))
    path_A.extend(segment if not path_A else segment[1:])
    current = positions[target]

# B차 경로 (A차 예약 회피)
reserved_B = build_reserved(path_A)
path_B = []
current = positions[b_order[0]]
for target in b_order[1:]:
    segment = astar_time_reserved(current, positions[target], grid, reserved_B)
    reserved_B.update(build_reserved(segment))
    path_B.extend(segment if not path_B else segment[1:])
    current = positions[target]

# 목적지에서 정지시간 반영
a_stops = {positions[c] for c in a_order if c != 'A'}
b_stops = {positions[c] for c in b_order if c != 'B'}
flat_A = flatten_path_with_stops(path_A, a_stops, stay_time=3)
flat_B = flatten_path_with_stops(path_B, b_stops, stay_time=2)

simulate_paths(
    flat_A, flat_B,
    [c for c in a_order if c != 'A'],
    [c for c in b_order if c != 'B'],
    positions['A'], positions['B'],
    positions['A'], positions['B']
)
