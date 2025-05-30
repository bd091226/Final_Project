#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <unistd.h>

#define ROWS 9
#define COLS 9
#define MAX_NODES (ROWS*COLS)
#define INF INT_MAX
#define SLEEP_USEC 500000  // 0.5 seconds
#define PREDICT_RANGE 3

// Grid cell: 0 = free, 1 = wall/block, >1 negative ASCII label for landmarks
// We'll store landmarks as negative 'A', 'B', 'S', 'G', 'K', 'W' etc.

// 맵 상의 위치를 나타내는 행과 열
typedef struct 
{ 
    int r, c; 
} Point; 

// A* 알고리즘에 사용되는 노드는 나타내는 구조
typedef struct 
{ 
    int f, g; // f = 총비용(g+h)을 저장, g = 시작점에서 현재 노드까지의 실제 비용을 저장
    Point pt; // 현재 노드의 위치를 저장
} Node;

// Min-heap for open set
Node heap[MAX_NODES];
int heap_size; // 힙에 저장된 노드의 수를 추적

void heap_swap(Node *a, Node *b) // 두 노드의 위치를 교환하는 함수
{ 
    Node t=*a; 
    *a=*b; 
    *b=t; 
}

// * 힙 : 완전 이진 트리의 일종으로 우선 순위 큐를 위하여 만들어진 자료구조
// 여러 개의 값들 중에 최댓값이나 최솟값을 빠르게 찾아내도록 만들어짐
void heap_push(Node n) // 힙에 새로운 노드를 추가, 최소 힙 속성을 유지하도록 재정렬
{
    int i = ++heap_size; // 새노드를 힙의 맨끝(heap_size)에 삽입 
    heap[i] = n;
    while (i>1 && heap[i/2].f > heap[i].f) // 부모와 비교하여 f값이 더 작으면 위로 올려 최소 힙 속성유지
    {
        heap_swap(&heap[i], &heap[i/2]);
        i /= 2;
    }
}

Node heap_pop() // 힙에서 최소 f 값을 가진 노드를 제거하고 반환, 그 후 힙 속성 유지하도록 재정렬
{
    Node top = heap[1];
    heap[1] = heap[heap_size--];
    int i=1;
    while (1) {
        int l=2*i, r=2*i+1, smallest=i;
        if (l<=heap_size && heap[l].f<heap[smallest].f) smallest=l;
        if (r<=heap_size && heap[r].f<heap[smallest].f) smallest=r;
        if (smallest==i) break;
        heap_swap(&heap[i], &heap[smallest]);
        i = smallest;
    }
    return top;
}

int heuristic(Point a, Point b) // 휴리스틱 함수, 맨해튼 거리(행 차이 + 열 차이)를 계산하여 반환
{ return abs(a.r-b.r)+abs(a.c-b.c); }
int in_bounds(int r, int c) // 주어진 좌표가 그리드 내에 있는지 확인
{ return r>=0 && r<ROWS && c>=0 && c<COLS; }
int is_walkable(int grid[ROWS][COLS], int r, int c) //해당 위치가 이동 가능한지 확인, 0이거나 양수인 경우 이동가능하다고 판단
{ return grid[r][c]==0 || grid[r][c]<0; }

// A* pathfinder: returns number of steps, writes path into out_path array
int astar(int grid[ROWS][COLS], Point start, Point goal, Point *out_path) {
    heap_size=0;
    int g_score[MAX_NODES]; // 시적점에서 각 노드까지의 최단 거리 값을 저장
    Point came_from[MAX_NODES]; // 각 노드에 도달하기 위한 이전 노드를 저장 -> 역추적용
    for (int i=0;i<MAX_NODES;i++) g_score[i]=INF;
    
    g_score[start.r*COLS + start.c] = 0;
    heap_push((Node){.f=heuristic(start,goal), .g=0, .pt=start}); // 휴리스틱만큼 f 설정한 후 힙에 삽입

    while (heap_size>0) 
    {
        Node cur = heap_pop();
        Point u = cur.pt;
        if (u.r==goal.r && u.c==goal.c) // 꺼낸 노드가 목표점이라면 재구성 진입
        {
            int len=0;
            Point p = goal;
            while (!(p.r==start.r && p.c==start.c)) 
            {
                out_path[len++] = p;
                Point prev = came_from[p.r*COLS + p.c]; // 배열을 따라 목표점에서 시작점까지 역추적
                p = prev;
            }
            out_path[len++] = start;
            // reverse
            for (int i=0;i<len/2;i++) // 저장된 경로 반전하여 올바른 순서로 만들기
            {
                Point t = out_path[i]; out_path[i]=out_path[len-1-i]; out_path[len-1-i]=t;
            }
            return len;
        }
        //이웃 노드 탐색 및 처리
        int dirs[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
        //상하좌우 4방향을 순차 탐색
        for (int i=0;i<4;i++) {
            int nr=u.r+dirs[i][0], nc=u.c+dirs[i][1];
            if (in_bounds(nr,nc) && is_walkable(grid,nr,nc)) // 유효 좌표 and 이동가능
            {
                int tent_g = cur.g + 1;
                int idx = nr*COLS + nc;
                if (tent_g < g_score[idx]) { // 더 짧은 경로이면
                    g_score[idx] = tent_g; 
                    came_from[idx] = u;
                    Node nb = {.g=tent_g, .f=tent_g + heuristic((Point){nr,nc}, goal), .pt={nr,nc}}; // 새로운 노드
                    heap_push(nb); //삽입
                }
            }
        }
    }
    return 0;
}


void print_grid(int grid[ROWS][COLS], Point A, Point B) // 시각화
{
    for (int r=0;r<ROWS;r++){
        for (int c=0;c<COLS;c++){
            if (r==A.r && c==A.c && r==B.r && c==B.c) putchar('X');
            else if (r==A.r && c==A.c) putchar('A');
            else if (r==B.r && c==B.c) putchar('B');
            else if (grid[r][c]==1) putchar('#');
            else if (grid[r][c]<0) putchar((char)(-grid[r][c]));
            else putchar('.');
            putchar(' ');
        }
        putchar('\n');
    }
    putchar('\n');
}

int point_equals(Point a, Point b){return a.r==b.r && a.c==b.c;} // 두 좌표가 동일한지 

int main(){
    // raw grid: negatives indicate labels
    int raw[ROWS][COLS] = {
        {- 'B',0,0,0,0,0,0,0,0},
        {0,1,1,1,0,1,1,1,0},
        {0,1,1,1,0,1,1,1,0},
        {0,1,- 'S',1,0,1,- 'G',1,0},
        {0,0,0,0,0,0,0,0,0},
        {0,1,1,1,0,1,1,1,0},
        {0,1,1,1,0,1,1,1,0},
        {0,1,- 'K',1,0,1,- 'W',1,0},
        {0,0,0,0,0,0,0,0,- 'A'}
    };
    
    int grid[ROWS][COLS];
    Point coords[256] = {0};
    // map coords and copy grid
    for (int r=0;r<ROWS;r++) for (int c=0;c<COLS;c++){
        grid[r][c] = raw[r][c]==1 ? 1 : (raw[r][c]<0 ? raw[r][c] : 0);
        if (raw[r][c]<0){ coords[(unsigned char)(-raw[r][c])] = (Point){r,c}; }
    }
    
    // define goals
    char goalsA[] = {'K','G','S','W','A'};
    int lenGA = sizeof(goalsA)/sizeof(goalsA[0]);
    char goalsB[] = {'W','B','S','B','G','B','K','B'};
    int lenGB = sizeof(goalsB)/sizeof(goalsB[0]);

    // exit zone map for B static futures
    Point exit_zone_map[256][4];
    int ez_count[256]={0};
    // S->(4,2), G->(4,6), K->(8,2), W->(8,6)
    ez_count['S']=1; exit_zone_map['S'][0] = coords['S'];
    ez_count['G']=1; exit_zone_map['G'][0] = coords['G'];
    ez_count['K']=1; exit_zone_map['K'][0] = coords['K'];
    ez_count['W']=1; exit_zone_map['W'][0] = coords['W'];

    // state
    Point A = coords['A'], B = coords['B'];
    int idxA=0, idxB=0;
    Point pathA[MAX_NODES], pathB[MAX_NODES];
    int lenA=1, lenB=1;
    pathA[0]=A; pathB[0]=B;
    // track blocks for B
    int block_map[ROWS][COLS]={0};
    Point block_list[MAX_NODES]; int block_cnt=0;
    int A_blocking=0;

    int t=0;
    while (idxA < lenGA || idxB < lenGB) {
        // plan A
        if (lenA<=1 && idxA<lenGA) {
            Point dest = coords[(unsigned char)goalsA[idxA++]];
            lenA = astar(grid, A, dest, pathA);
        }
        // predict B future moves
        Point futureB[PREDICT_RANGE + 4]; int fb_cnt=0;
        if (lenB>1) {
            for (int i=1;i<1+PREDICT_RANGE && i<lenB;i++) futureB[fb_cnt++] = pathB[i];
        }
        // static after reaching last goal
        if (idxB>0) {
            char last = goalsB[idxB-1];
            for (int i=0;i<ez_count[(unsigned char)last];i++) futureB[fb_cnt++] = exit_zone_map[(unsigned char)last][i];
        }
        // next intended steps
        Point nextA = (lenA>1?pathA[1]:A);
        Point nextB = (lenB>1?pathB[1]:B);
        int moveA=1, moveB=1;
        // collision rules
        if (point_equals(nextA,nextB)) moveA=0;
        if (point_equals(nextA,B) && point_equals(nextB,A)) moveA=0;
        for (int i=0;i<fb_cnt;i++) {
            if (point_equals(nextA, futureB[i])) moveA=0;
        }
        for (int i=PREDICT_RANGE; i<fb_cnt; i++) {
            if (point_equals(nextA, futureB[i])) {
                int dA = abs(nextA.r-A.r)+abs(nextA.c-A.c);
                int dB = abs(nextA.r-B.r)+abs(nextA.c-B.c);
                if (dB <= dA) moveA=0;
            }
        }
        if (lenB>1) {
            for (int i=1;i<1+PREDICT_RANGE && i<lenB;i++) if (point_equals(A, pathB[i])) moveA=0;
        }
        // A blocking
        if (!moveA) {
            A_blocking=1;
            // add block
            if (!block_map[A.r][A.c]) {
                block_map[A.r][A.c]=1;
                block_list[block_cnt++] = A;
            }
            // try alt path away from futureB
            int retreatLen=0; Point retreatPath[MAX_NODES];
            retreatLen = astar(grid, A, pathA[0], retreatPath);
            if (retreatLen>1) {
                Point cand = retreatPath[1]; int conflict=0;
                for (int i=0;i<fb_cnt;i++) if (point_equals(cand, futureB[i])) conflict=1;
                if (!conflict) {
                    // switch to alt
                    lenA = retreatLen;
                    memcpy(pathA, retreatPath, retreatLen*sizeof(Point));
                    moveA=1;
                }
            }
        } else {
            A_blocking=0;
            if (block_map[A.r][A.c]) block_map[A.r][A.c]=0;
        }
        // B replan if needed
        if ((lenB<=1 && idxB<lenGB) || A_blocking) {
            // mark temp grid
            int temp[ROWS][COLS]; memcpy(temp, grid, sizeof(grid));
            for (int i=0;i<block_cnt;i++) temp[block_list[i].r][block_list[i].c]=1;
            if (idxB < lenGB) {
                Point dest = coords[(unsigned char)goalsB[idxB]];
                lenB = astar(temp, B, dest, pathB);
                if (lenB<=1) idxB++;
            }
        }
        // perform moves
        if (moveA && lenA>1) { A=pathA[1]; memmove(pathA, pathA+1, (--lenA)*sizeof(Point)); }
        if (moveB && lenB>1) { B=pathB[1]; memmove(pathB, pathB+1, (--lenB)*sizeof(Point)); }

        printf("--- t=%d ---\n", t++);
        print_grid(grid, A, B);
        usleep(SLEEP_USEC);
    }
    return 0;
}
