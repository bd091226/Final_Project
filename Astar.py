

import heapq
import time

# A* 경로 탐색
def astar(grid, start, goal):
    rows, cols = len(grid), len(grid[0])
    def h(a, b):
        return abs(a[0] - b[0]) + abs(a[1] - b[1])
    
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
        output[r][c] = 'X'  # 충돌 표시
    else:
        rA, cA = posA
        rB, cB = posB
        if output[rA][cA] not in ('S','W','K','G','B'): output[rA][cA] = 'A'
        if output[rB][cB] not in ('S','W','K','G','A'): output[rB][cB] = 'B'

    for row in output:
        print(' '.join(str(x) for x in row))
    print()

# 시뮬레이션 (인덱스 기반 충돌 대기 로직 추가)
def simulate_A_B(grid, pathA, pathB):
    idxA, idxB = 0, 0
    posA = pathA[0]
    posB = pathB[0]
    t = 0

    while idxA < len(pathA) - 1 or idxB < len(pathB) - 1:
        # 다음에 가려고 하는 위치
        nextA = pathA[idxA+1] if idxA < len(pathA) - 1 else posA
        nextB = pathB[idxB+1] if idxB < len(pathB) - 1 else posB

        # 충돌 검사: 두 캐릭터가 같은 칸으로 가려 하면
        if nextA == nextB:
            # A만 먼저 이동, B는 대기
            posA = nextA
            idxA += 1
            # B는 idxB, posB 변경 없음
        else:
            # 충돌 없으면 둘 다 이동
            if idxA < len(pathA) - 1:
                posA = nextA
                idxA += 1
            if idxB < len(pathB) - 1:
                posB = nextB
                idxB += 1

        # 지나간 자리 복원
        if t > 0:
            prevA = pathA[idxA-1]
            if prevA != posA and isinstance(grid[prevA[0]][prevA[1]], int):
                grid[prevA[0]][prevA[1]] = 0
            prevB = pathB[idxB-1]
            if prevB != posB and isinstance(grid[prevB[0]][prevB[1]], int):
                grid[prevB[0]][prevB[1]] = 0

        # 출력
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

    # 좌표 매핑
    coords = {v:(i,j) for i,row in enumerate(grid) for j,v in enumerate(row) if isinstance(v,str)}
    startA = coords['A']
    startB = coords['B']

# A -> S -> W -> K -> G ->  A
# B -> G -> B -> S -> B -> W -> B -> K -> B
    goalsA = ['S', 'W', 'K','G','A']
    goalsB = ['G', 'B', 'S','B','W','B','K','B']

    # A 전체 경로
    fullA = []
    curr = startA
    for dest in goalsA:
        seg = astar(grid, curr, coords[dest])
        if seg is None:
            print(f"A 경로 없음: {curr} → {coords[dest]}")
            exit()
        fullA += seg[1:] if fullA else seg
        curr = coords[dest]

    # **B 경로: 원래 grid 그대로 사용**
    fullB = []
    curr = startB
    for dest in goalsB:
        seg = astar(grid, curr, coords[dest])
        if seg is None:
            print(f"B 경로 없음: {curr} → {coords[dest]}")
            exit()
        fullB += seg[1:] if fullB else seg
        curr = coords[dest]

    # 시뮬레이션 시작
    simulate_A_B(grid, fullA, fullB)
