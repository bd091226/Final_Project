import heapq
import time

# A* 경로 탐색
def astar(grid, start, goal):
    rows, cols = len(grid), len(grid[0])
    def h(a, b): return abs(a[0] - b[0]) + abs(a[1] - b[1])

    open_set = []
    heapq.heappush(open_set, (h(start, goal), 0, start))
    came_from = {}
    g_score = {start: 0}

    while open_set:
        f, g, current = heapq.heappop(open_set)
        if current == goal:
            path = []
            while current in came_from:
                path.append(current)
                current = came_from[current]
            path.append(start)
            return path[::-1]

        for d in [(1,0),(-1,0),(0,1),(0,-1)]:
            nr, nc = current[0]+d[0], current[1]+d[1]
            if 0 <= nr < rows and 0 <= nc < cols and (grid[nr][nc] == 0 or isinstance(grid[nr][nc], str)):
                neighbor = (nr, nc)
                tentative_g = g + 1
                if tentative_g < g_score.get(neighbor, float('inf')):
                    came_from[neighbor] = current
                    g_score[neighbor] = tentative_g
                    f_score = tentative_g + h(neighbor, goal)
                    heapq.heappush(open_set, (f_score, tentative_g, neighbor))
    return None

# 격자 출력
def print_grid(grid, posA, posB):
    output = [row[:] for row in grid]
    if posA == posB:
        r, c = posA
        output[r][c] = 'X'
    else:
        rA, cA = posA
        rB, cB = posB
        if output[rA][cA] not in ('S','W','K','G','B'): output[rA][cA] = 'A'
        if output[rB][cB] not in ('S','W','K','G','A'): output[rB][cB] = 'B'
    for row in output:
        print(' '.join(str(x) for x in row))
    print()

# 시뮬레이션
def simulate_A_B(grid, goalsA, goalsB, coords):
    posA = coords['A']
    posB = coords['B']
    idxAgoal = 0
    idxBgoal = 0
    pathA, pathB = [posA], [posB]
    t = 0

    exit_zone_map = {
        coords['G']: [(4,6)],
        coords['S']: [(4,2)],
        coords['K']: [(8,2)],
        coords['W']: [(8,6)],
    }

    while idxAgoal < len(goalsA) or idxBgoal < len(goalsB):
        # 목표 도달 시 경로 재계산
        if len(pathA) <= 1 and idxAgoal < len(goalsA):
            dest = coords[goalsA[idxAgoal]]
            seg = astar(grid, posA, dest)
            if seg: pathA = seg
            idxAgoal += 1
        if len(pathB) <= 1 and idxBgoal < len(goalsB):
            dest = coords[goalsB[idxBgoal]]
            seg = astar(grid, posB, dest)
            if seg: pathB = seg
            idxBgoal += 1

        # 예측 범위
        PREDICT_RANGE = 3
        futureB = pathB[1:1 + PREDICT_RANGE]

        # 출구 예측 포함
        if idxBgoal > 0:
            last_goal = coords[goalsB[idxBgoal - 1]]
            if last_goal in exit_zone_map:
                futureB += exit_zone_map[last_goal]

        nextA = pathA[1] if len(pathA) > 1 else posA
        nextB = pathB[1] if len(pathB) > 1 else posB

        moveA = True
        moveB = True

        # 충돌 조건 판단
        if nextA == nextB:
            moveA = False
        elif nextA == posB and nextB == posA:
            moveA = False
        elif nextA in futureB:
            moveA = False
        elif posA in futureB:
            moveA = False

        # 대기 위험 판단 → 우회 시도 or 후퇴
        if not moveA:
            if posA in futureB:
                # 위험한 대기 → 우회 시도
                dest = coords[goalsA[idxAgoal - 1]]
                alt = astar(grid, posA, dest)
                if alt and alt[1] not in futureB:
                    pathA = alt
                    moveA = True
                else:
                    # 우회 실패 → 후퇴
                    if posA in pathA:
                        idx = pathA.index(posA)
                        safe_idx = max(0, idx - 1)
                        posA = pathA[safe_idx]
                        pathA = [posA] + pathA[safe_idx+1:]

        # 이동
        if moveA and len(pathA) > 1:
            posA = pathA[1]
            pathA = pathA[1:]
        if moveB and len(pathB) > 1:
            posB = pathB[1]
            pathB = pathB[1:]

        print(f"--- t={t} ---")
        print_grid(grid, posA, posB)
        t += 1
        time.sleep(0.5)

# 메인
if __name__ == '__main__':
    grid = [
        ['B',0,0,0,0,0,0,0,0],
        [0,1,1,1,0,1,1,1,0],
        [0,1,1,1,0,1,1,1,0],
        [0,1,'S',1,0,1,'G',1,0],
        [0,0,0,0,0,0,0,0,0],
        [0,1,1,1,0,1,1,1,0],
        [0,1,1,1,0,1,1,1,0],
        [0,1,'K',1,0,1,'W',1,0],
        [0,0,0,0,0,0,0,0,'A'],
    ]

    coords = {v:(i,j) for i,row in enumerate(grid) for j,v in enumerate(row) if isinstance(v,str)}
    goalsA = ['S', 'G', 'K','W','A']
    goalsB = ['G', 'B', 'S','B','W','B','K','B']

    simulate_A_B(grid, goalsA, goalsB, coords)
