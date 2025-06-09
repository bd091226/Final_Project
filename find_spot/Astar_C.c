/*
 * Astar_C.c
 * A* controller that moves A completely, then B.
 * Uses A_motor_time and B_motor_time via pipes.
 *
 * Compile:
 *   gcc Astar_C.c -o Astar_C
 * Run:
 *   ./Astar_C
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <limits.h>
 #include <unistd.h>
 #include <sys/types.h>
 #include <sys/wait.h>
 #include <sys/select.h>
 #include <stdbool.h>
 
 #define ROWS 9
 #define COLS 9
 #define MAX_NODES (ROWS*COLS)
 #define INF INT_MAX
 #define BUF_SIZE 128
 #define CMD_TIMEOUT_SEC 1  // 응답 타임아웃(초)
 
 typedef struct { int r, c; } Point;
 typedef struct { int f, g; Point pt; } Node;
 
 // 전역 coords와 랜드마크
 static Point coords[256];
 static const char landmarks[] = { 'S','G','K','W' };
 static const int n_landmarks = sizeof(landmarks)/sizeof(landmarks[0]);
 
 static Node heap[MAX_NODES];
 static int heap_size;
 
 // 최소 힙 보조함수
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
     return abs(a.r - b.r) + abs(a.c - b.c);
 }
 static int in_bounds(int r, int c) {
     return r >= 0 && r < ROWS && c >= 0 && c < COLS;
 }
 static int is_walkable(int grid[ROWS][COLS], int r, int c) {
     return grid[r][c] == 0; // 장애물(1)이 아니면 걸어다닐 수 있음
 }
 
 // A* 경로탐색
 int astar(int grid[ROWS][COLS], Point start, Point goal, Point *out) {
     heap_size = 0;
     static int gs[MAX_NODES];
     static Point from[MAX_NODES];
     for (int i = 0; i < MAX_NODES; i++) gs[i] = INF;
     gs[start.r*COLS + start.c] = 0;
     heap_push((Node){ .f = heuristic(start, goal), .g = 0, .pt = start });
 
     while (heap_size) {
         Node cur = heap_pop();
         Point u = cur.pt;
         if (u.r == goal.r && u.c == goal.c) {
             int len = 0;
             Point p = goal;
             while (!(p.r == start.r && p.c == start.c)) {
                 out[len++] = p;
                 p = from[p.r*COLS + p.c];
             }
             out[len++] = start;
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
             if (in_bounds(nr, nc) && is_walkable(grid, nr, nc)) {
                 int ng = cur.g + 1, idx = nr*COLS + nc;
                 if (ng < gs[idx]) {
                     gs[idx] = ng;
                     from[idx] = u;
                     heap_push((Node){
                         .g = ng,
                         .f = ng + heuristic((Point){nr,nc}, goal),
                         .pt = {nr,nc}
                     });
                 }
             }
         }
     }
     return 0;
 }
 
 // 명령 전송 후 POS 응답 수신
 int send_cmd(FILE *w, FILE *r,
              const char *label, const char *cmd,
              Point *pos, int *dir) {
     char buf[BUF_SIZE];
     fd_set rfds; struct timeval tv;
 
     fprintf(w, "%s:%s\n", label, cmd);
     fflush(w);
 
     FD_ZERO(&rfds);
     FD_SET(fileno(r), &rfds);
     tv.tv_sec = CMD_TIMEOUT_SEC; tv.tv_usec = 0;
     if (select(fileno(r)+1, &rfds, NULL, NULL, &tv) <= 0) return -1;
 
     if (!fgets(buf, BUF_SIZE, r)) return -1;
     printf("[A_motor_time] %s", buf);
     fflush(stdout);
     int pr, pc, pd;
     if (sscanf(buf, "%*[^:]:POS %d %d %d", &pr, &pc, &pd) == 3) {
         if (pos) { pos->r = pr; pos->c = pc; }
         *dir = pd;
         return 0;
     }
     return -1;
 }
 
 // 그리드 출력 (열/행 라벨 + 랜드마크)
 void print_grid(int grid[ROWS][COLS], Point A, Point B) {
     printf("   ");
     for (int c = 0; c < COLS; c++) printf("%d ", c);
     printf("\n");
     for (int r = 0; r < ROWS; r++) {
         printf("%d: ", r);
         for (int c = 0; c < COLS; c++) {
             char ch = '.';
             if      (r==A.r && c==A.c)     ch = 'A';
             else if (r==B.r && c==B.c)     ch = 'B';
             else if (grid[r][c] == 1)      ch = '#';
             else {
                 for (int i = 0; i < n_landmarks; i++) {
                     char key = landmarks[i];
                     Point p = coords[(unsigned char)key];
                     if (p.r == r && p.c == c) {
                         ch = key;
                         break;
                     }
                 }
             }
             printf("%c ", ch);
         }
         printf("\n");
     }
     printf("\n");
 }
 
 int main() {
     printf("Astar_C 시작\n"); fflush(stdout);
 
     int pa_in[2], pa_out[2], pb_in[2], pb_out[2];
     pipe(pa_in); pipe(pa_out);
     pipe(pb_in); pipe(pb_out);
 
     if (fork() == 0) {
         dup2(pa_in[0], STDIN_FILENO);
         dup2(pa_out[1], STDOUT_FILENO);
         close(pa_in[1]); close(pa_out[0]);
         execl("./A_motor_time","A_motor_time",NULL);
         perror("execl A_motor_time"); exit(1);
     }
     close(pa_in[0]); close(pa_out[1]);
 
     if (fork() == 0) {
         dup2(pb_in[0], STDIN_FILENO);
         dup2(pb_out[1], STDOUT_FILENO);
         close(pb_in[1]); close(pb_out[0]);
         execl("./B_motor_time","B_motor_time",NULL);
         perror("execl B_motor_time"); exit(1);
     }
     close(pb_in[0]); close(pb_out[1]);
 
     FILE *fa_w = fdopen(pa_in[1],"w"), *fa_r = fdopen(pa_out[0],"r");
     FILE *fb_w = fdopen(pb_in[1],"w"), *fb_r = fdopen(pb_out[0],"r");
 
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
     for (int r=0; r<ROWS; r++) {
         for (int c=0; c<COLS; c++) {
             if (raw[r][c] == 1) {
                 grid[r][c] = 1;
             } else {
                 grid[r][c] = 0;
                 if (raw[r][c] < 0) {
                     coords[(unsigned char)(-raw[r][c])] = (Point){r,c};
                 }
             }
         }
     }
 
     char goalsA[] = {'K','G','S','W','A'}; int nA=5;
     char goalsB[] = {'W','B','S','B','G','B','K','B'}; int nB=8;
     Point posA = coords['A'], posB = coords['B'];
     int dirA=0, dirB=0, lenA=0, lenB=0, idxA=0, idxB=0;
     Point pathA[MAX_NODES], pathB[MAX_NODES];
     Point dummy;  // 모터 pos 피드백 무시용
 
     send_cmd(fa_w, fa_r, "A", "POS", &dummy, &dirA);
     send_cmd(fb_w, fb_r, "B", "POS", &dummy, &dirB);
     print_grid(grid, posA, posB);
 
     printf("--- Moving A ---\n");
     while (idxA < nA) {
         if (lenA <= 1) {
             lenA = astar(grid, posA, coords[(unsigned char)goalsA[idxA]], pathA);
             printf("→ Path to '%c' (len=%d): ", goalsA[idxA], lenA);
             for (int i=0; i<lenA; i++)
                 printf("(%d,%d) ", pathA[i].r, pathA[i].c);
             printf("\n");
             if (lenA <= 1) { idxA++; continue; }
         }
         Point nxt = pathA[1];
         int td = (nxt.r < posA.r ? 0 : nxt.r > posA.r ? 2 : nxt.c > posA.c ? 1 : 3);
         int diff = (td - dirA + 4) % 4; if (diff == 3) diff = -1;
         dirA = (dirA + diff + 4) % 4;
         if (diff < 0)
             send_cmd(fa_w, fa_r, "A", "TURN_LEFT", &dummy, &dirA);
         else if (diff > 0)
             send_cmd(fa_w, fa_r, "A", "TURN_RIGHT", &dummy, &dirA);
         print_grid(grid, posA, posB);
         usleep(500000);
 
         send_cmd(fa_w, fa_r, "A", "FORWARD", &dummy, &dirA);
         posA = nxt;
         memmove(pathA, pathA+1, (--lenA)*sizeof(Point));
         print_grid(grid, posA, posB);
         usleep(500000);
     }
 
     printf("--- Moving B ---\n");
     while (idxB < nB) {
         if (lenB <= 1) {
             lenB = astar(grid, posB, coords[(unsigned char)goalsB[idxB]], pathB);
             printf("→ Path to '%c' (len=%d): ", goalsB[idxB], lenB);
             for (int i=0; i<lenB; i++)
                 printf("(%d,%d) ", pathB[i].r, pathB[i].c);
             printf("\n");
             if (lenB <= 1) { idxB++; continue; }
         }
         Point nxt = pathB[1];
         int td = (nxt.r < posB.r ? 0 : nxt.r > posB.r ? 2 : nxt.c > posB.c ? 1 : 3);
         int diff = (td - dirB + 4) % 4; if (diff == 3) diff = -1;
         dirB = (dirB + diff + 4) % 4;
         if (diff < 0)
             send_cmd(fb_w, fb_r, "B", "TURN_LEFT", &dummy, &dirB);
         else if (diff > 0)
             send_cmd(fb_w, fb_r, "B", "TURN_RIGHT", &dummy, &dirB);
         print_grid(grid, posA, posB);
         usleep(500000);
 
         send_cmd(fb_w, fb_r, "B", "FORWARD", &dummy, &dirB);
         posB = nxt;
         memmove(pathB, pathB+1, (--lenB)*sizeof(Point));
         print_grid(grid, posA, posB);
         usleep(500000);
     }
 
     wait(NULL);
     wait(NULL);
     return 0;
 } 