/*
 * Astar_C.c
 * 회전(TURN)과 전진(FORWARD)을 분리한 예측+차단+후퇴 전략
 * A와 B 모두 화살표로 표시됩니다.
 *
 * 컴파일:
 *   gcc Astar_C.c -o Astar_C
 * 실행:
 *   ./Astar_C
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/select.h>
#include <stdbool.h>

#define NORTH 0
#define EAST  1
#define SOUTH 2
#define WEST  3

#define ROWS 9
#define COLS 9
#define MAX_NODES (ROWS * COLS)
#define INF INT_MAX
#define SLEEP_USEC 500000    // 0.5초
#define PREDICT_RANGE 3      // 미래 몇 칸 예측
#define BUF_SIZE 128
#define CMD_TIMEOUT_SEC 1

typedef struct { int r, c; } Point;
typedef struct { int f, g; Point pt; } Node;

// 전역 랜드마크 좌표 저장
static Point coords[256];

// A*용 간단 최소 힙
static Node heap[MAX_NODES+1];
static int heap_size;
static void heap_swap(Node *a, Node *b) { Node t = *a; *a = *b; *b = t; }
static void heap_push(Node n) {
    int i = ++heap_size;
    heap[i] = n;
    while (i > 1 && heap[i/2].f > heap[i].f) {
        heap_swap(&heap[i], &heap[i/2]);
        i /= 2;
    }
}
static Node heap_pop(void) {
    Node top = heap[1];
    heap[1] = heap[heap_size--];
    int i = 1;
    for (;;) {
        int l = 2*i, r = 2*i+1, s = i;
        if (l <= heap_size && heap[l].f < heap[s].f) s = l;
        if (r <= heap_size && heap[r].f < heap[s].f) s = r;
        if (s == i) break;
        heap_swap(&heap[i], &heap[s]);
        i = s;
    }
    return top;
}

static int heuristic(Point a, Point b) {
    // 맨해튼 거리 휴리스틱
    return abs(a.r - b.r) + abs(a.c - b.c);
}
static int in_bounds(int r, int c) {
    // 그리드 내에 있는지 확인
    return r >= 0 && r < ROWS && c >= 0 && c < COLS;
}
static int is_walkable(int grid[ROWS][COLS], int r, int c) {
    // 0 이거나 랜드마크 음수일 때 이동 가능
    return grid[r][c] == 0 || grid[r][c] < 0;
}

// A* 경로 탐색: 경로 길이 반환, 경로는 out[]에 저장
int astar(int grid[ROWS][COLS], Point start, Point goal, Point *out) {
    heap_size = 0;
    static int g_score[MAX_NODES];
    static Point came_from[MAX_NODES];
    for (int i = 0; i < MAX_NODES; i++) g_score[i] = INF;
    int sidx = start.r * COLS + start.c;
    g_score[sidx] = 0;
    heap_push((Node){ .f = heuristic(start,goal), .g = 0, .pt = start });

    while (heap_size) {
        Node cur = heap_pop();
        Point u = cur.pt;
        if (u.r == goal.r && u.c == goal.c) {
            int len = 0;
            Point p = goal;
            while (!(p.r == start.r && p.c == start.c)) {
                out[len++] = p;
                p = came_from[p.r*COLS + p.c];
            }
            out[len++] = start;
            // 역순 뒤집기
            for (int i = 0; i < len/2; i++) {
                Point t = out[i];
                out[i] = out[len-1-i];
                out[len-1-i] = t;
            }
            return len;
        }
        int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        for (int i = 0; i < 4; i++) {
            int nr = u.r + dirs[i][0], nc = u.c + dirs[i][1];
            if (in_bounds(nr,nc) && is_walkable(grid,nr,nc)) {
                int idx = nr*COLS + nc;
                int ng = cur.g + 1;
                if (ng < g_score[idx]) {
                    g_score[idx] = ng;
                    came_from[idx] = u;
                    heap_push((Node){
                        .g = ng,
                        .f = ng + heuristic((Point){nr,nc}, goal),
                        .pt = {nr,nc}
                    });
                }
            }
        }
    }
    return 0;  // 경로 없음
}

// A/B 모터 프로세스와 통신
int send_cmd(FILE *w, FILE *r,
    const char *label, const char *cmd,
    Point *pos, int *dir)
{
    char buf[BUF_SIZE];
    fd_set rfds; struct timeval tv;

    // 1) 명령 전송
    fprintf(w, "%s:%s\n", label, cmd);
    fflush(w);

    // 2) 응답 대기 (select)
    FD_ZERO(&rfds);
    FD_SET(fileno(r), &rfds);
    tv.tv_sec = CMD_TIMEOUT_SEC; tv.tv_usec = 0;
    if (select(fileno(r)+1, &rfds, NULL, NULL, &tv) <= 0)
    return -1;

    // 3) POS 줄을 만날 때까지 읽기
    while (fgets(buf, BUF_SIZE, r)) {
    printf("%s", buf);

    // buf 안에 ":POS " 가 있으면 그 줄을 파싱
    int pr, pc, pd;
    if (sscanf(buf, "%*[^:]:POS %d %d %d", &pr, &pc, &pd) == 3) {
        pos->r = pr;
        pos->c = pc;
        *dir  = pd;
        return 0;
        }
    }
    return -1;  // EOF 나 에러
}

// 그리드 출력: A, B 모두 화살표로 표시
void print_grid(int grid[ROWS][COLS],
                Point A, int dirA,
                Point B, int dirB)
{
    char arrow[4] = {'^','>','v','<'};
    printf("   ");
    for (int c = 0; c < COLS; c++) printf("%d ", c);
    printf("\n");
    for (int r = 0; r < ROWS; r++) {
        printf("%d: ", r);
        for (int c = 0; c < COLS; c++) {
            char ch = '.';
            if (r == A.r && c == A.c)      ch = arrow[dirA];
            else if (r == B.r && c == B.c) ch = arrow[dirB];  // <-- B도 화살표로
            else if (grid[r][c] == 1)      ch = '#';
            else if (grid[r][c] < 0)       ch = (char)(-grid[r][c]);
            printf("%c ", ch);
        }
        printf("\n");
    }
    printf("\n");
}

int point_equals(Point a, Point b) {
    return a.r == b.r && a.c == b.c;
}

int main(){
    printf("Astar_C 시작\n");
    fflush(stdout);

    // 파이프 생성 및 A/B 모터 프로세스 포크
    int pa_in[2], pa_out[2], pb_in[2], pb_out[2];
    pipe(pa_in); pipe(pa_out);
    pipe(pb_in); pipe(pb_out);
    if(fork()==0){
        dup2(pa_in[0], STDIN_FILENO);
        dup2(pa_out[1], STDOUT_FILENO);
        close(pa_in[1]); close(pa_out[0]);
        execl("./A_motor_time","A_motor_time",NULL);
        perror("execl A_motor_time"); exit(1);
    }
    close(pa_in[0]); close(pa_out[1]);
    if(fork()==0){
        dup2(pb_in[0], STDIN_FILENO);
        dup2(pb_out[1], STDOUT_FILENO);
        close(pb_in[1]); close(pb_out[0]);
        execl("./B_motor_time","B_motor_time",NULL);
        perror("execl B_motor_time"); exit(1);
    }
    close(pb_in[0]); close(pb_out[1]);
    FILE *fa_w = fdopen(pa_in[1],"w"), *fa_r = fdopen(pa_out[0],"r");
    FILE *fb_w = fdopen(pb_in[1],"w"), *fb_r = fdopen(pb_out[0],"r");

    // 그리드 초기화 및 랜드마크 좌표 설정
    int raw[ROWS][COLS] = {
        { -'A',0,0,0,0,0,0,0,0 },
        {    0,1,1,1,0,1,1,1,0 },
        {    0,1,1,1,0,1,1,1,0 },
        {    0,1,-'S',1,0,1,-'G',1,0 },
        {    0,0,0,0,0,0,0,0,0 },
        {    0,1,1,1,0,1,1,1,0 },
        {    0,1,1,1,0,1,1,1,0 },
        {    0,1,-'K',1,0,1,-'W',1,0 },
        {    0,0,0,0,0,0,0,0,-'B' }
    };
    int grid[ROWS][COLS];
    for(int r=0;r<ROWS;r++){
        for(int c=0;c<COLS;c++){
            if(raw[r][c]==1) grid[r][c]=1;
            else {
                grid[r][c] = raw[r][c]<0 ? raw[r][c] : 0;
                if(raw[r][c]<0)
                    coords[(unsigned char)(-raw[r][c])] = (Point){r,c};
            }
        }
    }

    // 목표 리스트
    char goalsA[] = {'G','K','S','G','A'};
    char goalsB[] = {'W','B','K','B','G','B','K','B'};
    int nA = sizeof(goalsA)/sizeof(goalsA[0]);
    int nB = sizeof(goalsB)/sizeof(goalsB[0]);

    // B의 정적 종료 구역 설정
    static Point exit_zone_map[256][1];
    static int ez_count[256] = {0};
    ez_count['S']=1; exit_zone_map['S'][0]=coords['S'];
    ez_count['G']=1; exit_zone_map['G'][0]=coords['G'];
    ez_count['K']=1; exit_zone_map['K'][0]=coords['K'];
    ez_count['W']=1; exit_zone_map['W'][0]=coords['W'];

    // 초기 위치 요청 및 방향 초기화
    // A를 (0,0) 출발, B를 (8,8) 출발로 설정
    Point A = coords['A'], B = coords['B'], dummy;
    int dirA = SOUTH,    // A: 아래(SOUTH) 방향으로 시작
        dirB = NORTH;    // B: 위(NORTH) 방향으로 시작
    send_cmd(fa_w, fa_r, "A", "POS", &A, &dirA);
    send_cmd(fb_w, fb_r, "B", "POS", &B, &dirB);
    print_grid(grid, A, dirA, B, dirB);

    // 경로 버퍼 및 상태 변수
    Point pathA[MAX_NODES], pathB[MAX_NODES];
    int lenA=1, lenB=1, idxA=0, idxB=0;
    pathA[0]=A; pathB[0]=B;
    bool block_map[ROWS][COLS]={{false}};
    Point block_list[MAX_NODES]; int block_cnt=0;
    bool A_blocking=false;
    Point retreatPath[MAX_NODES]; int retreatLen=0;

    int t = 0;
    while (idxA < nA || idxB < nB) {
        // ── A 경로 재계획 ──
        if (lenA <= 1 && idxA < nA) {
            Point destA = coords[(unsigned char)goalsA[idxA++]];
            lenA = astar(grid, A, destA, pathA);
        }

        // ── B 경로 재계획 (A blocking 있을 때만) ──
        if ((lenB <= 1 && idxB < nB) || A_blocking) {
            int temp[ROWS][COLS];
            memcpy(temp, grid, sizeof(grid));
            for (int i = 0; i < block_cnt; i++) {
                Point p = block_list[i];
                temp[p.r][p.c] = 1;
            }
            if (idxB < nB) {
                Point destB = coords[(unsigned char)goalsB[idxB]];
                lenB = astar(temp, B, destB, pathB);
                if (lenB <= 1) idxB++;
            }
        }

        // ── B 미래 경로 예측 ──
        Point futureB[PREDICT_RANGE];
        int fb_cnt = 0;
        if (lenB > 1) {
            for (int k = 1; k <= PREDICT_RANGE && k < lenB; k++) {
                futureB[fb_cnt++] = pathB[k];
            }
        }

        // ── A 이동 결정 ──
        Point nextA = (lenA > 1 ? pathA[1] : A);
        Point nextB = (lenB > 1 ? pathB[1] : B);
        bool moveA = true;

        // 즉시 충돌 방지 (정면·스왑·B가 A 자리로 진입)
        if ( point_equals(nextA, nextB)
          || (point_equals(nextA, B) && point_equals(nextB, A))
          || point_equals(nextB, A) ) {
            moveA = false;
        }

        // 미래 충돌 예측: B의 다음 PREDICT_RANGE 칸 안에 A가 들어가려 하면 대기
        for (int i = 0; i < fb_cnt; i++) {
            if (point_equals(nextA, futureB[i])) {
                moveA = false;
                printf("[Predict] A waits at (%d,%d)\n", A.r, A.c);
                fflush(stdout);
                break;
            }
        }

        // ── A 실제 이동 ──
        if (moveA && lenA > 1) {
            Point nxt = pathA[1];
            int td = (nxt.r < A.r ? NORTH
                     : nxt.r > A.r ? SOUTH
                     : nxt.c > A.c ? EAST : WEST);
            int diff = (td - dirA + 4) % 4;
            if (diff == 3) diff = -1;
            printf("A diff: %d\n", diff); fflush(stdout);

            if (diff < 0) {
                send_cmd(fa_w, fa_r, "A", "TURN_LEFT", &dummy, &dirA);
            } else if (diff > 0) {
                send_cmd(fa_w, fa_r, "A", "TURN_RIGHT", &dummy, &dirA);
            } else {
                send_cmd(fa_w, fa_r, "A", "FORWARD", &dummy, &dirA);
                A = nxt;
                memmove(pathA, pathA+1, (--lenA) * sizeof(Point));
            }
        }
        // else: A waits, pathA/lenA/A unchanged

        // ── B 실제 이동 ──
        if (lenB > 1) {
            Point nxtB = pathB[1];
            int tdB = (nxtB.r < B.r ? NORTH
                      : nxtB.r > B.r ? SOUTH
                      : nxtB.c > B.c ? EAST : WEST);
            int diffB = (tdB - dirB + 4) % 4;
            if (diffB == 3) diffB = -1;
            printf("B diff: %d\n", diffB); fflush(stdout);

            if (diffB < 0) {
                send_cmd(fb_w, fb_r, "B", "TURN_LEFT", &dummy, &dirB);
            } else if (diffB > 0) {
                send_cmd(fb_w, fb_r, "B", "TURN_RIGHT", &dummy, &dirB);
            } else {
                send_cmd(fb_w, fb_r, "B", "FORWARD", &dummy, &dirB);
                B = nxtB;
                memmove(pathB, pathB+1, (--lenB) * sizeof(Point));
            }
        }

        // ── 한 스텝 완료 ──
        printf("--- t=%d ---\n", t++);
        print_grid(grid, A, dirA, B, dirB);
        usleep(SLEEP_USEC);
    }

    return 0;
}