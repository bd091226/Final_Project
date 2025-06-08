/*
 * Astar_C.c
 * A* pathfinding controller that spawns A_motor_time and B_motor_time
 * communicates via pipes: commands and position feedback
 *
 * 컴파일:
 *   gcc Astar_C.c -o Astar_C
 * 실행:
 *   ./Astar_C
 *
 * A_motor_time.c, B_motor_time.c 같은 디렉터리 안에 있어야함
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <limits.h>
 #include <unistd.h>
 #include <sys/types.h>
 #include <sys/wait.h>
 
 #define ROWS 9
 #define COLS 9
 #define MAX_NODES (ROWS*COLS)
 #define INF INT_MAX
 #define BUF_SIZE 128
 
 // Grid point
 typedef struct { int r, c; } Point;
 // Node for A*
 typedef struct { int f, g; Point pt; } Node;
 
 static Node heap[MAX_NODES];
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
 static Node heap_pop() {
     Node top = heap[1];
     heap[1] = heap[heap_size--];
     int i = 1;
     while (1) {
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
 static int is_walkable(int g[ROWS][COLS], int r, int c) {
     return g[r][c] == 0 || g[r][c] < 0;
 }
 
 int astar(int grid[ROWS][COLS], Point start, Point goal, Point *out) {
     heap_size = 0;
     static int gs[MAX_NODES];
     static Point from[MAX_NODES];
     for (int i = 0; i < MAX_NODES; i++) gs[i] = INF;
     gs[start.r * COLS + start.c] = 0;
     heap_push((Node){ .f = heuristic(start, goal), .g = 0, .pt = start });
     while (heap_size) {
         Node cur = heap_pop();
         Point u = cur.pt;
         if (u.r == goal.r && u.c == goal.c) {
             int len = 0;
             Point p = goal;
             while (!(p.r == start.r && p.c == start.c)) {
                 out[len++] = p;
                 p = from[p.r * COLS + p.c];
             }
             out[len++] = start;
             // reverse
             for (int i = 0; i < len/2; i++) {
                 Point t = out[i];
                 out[i] = out[len-1-i];
                 out[len-1-i] = t;
             }
             return len;
         }
         int dirs[4][2] = {{1,0}, {-1,0}, {0,1}, {0,-1}};
         for (int i = 0; i < 4; i++) {
             int nr = u.r + dirs[i][0], nc = u.c + dirs[i][1];
             if (in_bounds(nr, nc) && is_walkable(grid, nr, nc)) {
                 int ng = cur.g + 1;
                 int idx = nr * COLS + nc;
                 if (ng < gs[idx]) {
                     gs[idx] = ng;
                     from[idx] = u;
                     heap_push((Node){ .g = ng, .f = ng + heuristic((Point){nr, nc}, goal), .pt = {nr, nc} });
                 }
             }
         }
     }
     return 0;
 }
 
 // send command and echo motor output, then parse POS
 void send_cmd(FILE *w, FILE *r, const char *label, const char *cmd, Point *pos, int *dir) {
     char buf[BUF_SIZE];
     // send command
     fprintf(w, "%s:%s\n", label, cmd);
     fflush(w);
     // wait for response(s)
     while (fgets(buf, BUF_SIZE, r)) {
         // echo any motor controller logs
         printf("%s", buf);
         fflush(stdout);
         // parse position
         int pr, pc, pd;
         if (sscanf(buf, "%*[^:]:POS %d %d %d", &pr, &pc, &pd) == 3) {
             pos->r = pr;
             pos->c = pc;
             *dir = pd;
             break;
         }
     }
 }
 
 int main() {
     int pa_in[2], pa_out[2], pb_in[2], pb_out[2];
     pid_t pid;
     pipe(pa_in); pipe(pa_out);
     pipe(pb_in); pipe(pb_out);
     // spawn A_motor_time
     if ((pid = fork()) == 0) {
         dup2(pa_in[0], STDIN_FILENO);
         dup2(pa_out[1], STDOUT_FILENO);
         close(pa_in[1]); close(pa_out[0]);
         execl("./A_motor_time", "A_motor_time", NULL);
         perror("execl A_motor_time"); exit(1);
     }
     close(pa_in[0]); close(pa_out[1]);
     // spawn B_motor_time
     if ((pid = fork()) == 0) {
         dup2(pb_in[0], STDIN_FILENO);
         dup2(pb_out[1], STDOUT_FILENO);
         close(pb_in[1]); close(pb_out[0]);
         execl("./B_motor_time", "B_motor_time", NULL);
         perror("execl B_motor_time"); exit(1);
     }
     close(pb_in[0]); close(pb_out[1]);
     FILE *fa_w = fdopen(pa_in[1], "w");
     FILE *fa_r = fdopen(pa_out[0], "r");
     FILE *fb_w = fdopen(pb_in[1], "w");
     FILE *fb_r = fdopen(pb_out[0], "r");
 
     // initialize grid
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
     for (int r = 0; r < ROWS; r++) {
         for (int c = 0; c < COLS; c++) {
             grid[r][c] = (raw[r][c] == 1 ? 1 : (raw[r][c] < 0 ? raw[r][c] : 0));
             if (raw[r][c] < 0) coords[(unsigned char)(-raw[r][c])] = (Point){r, c};
         }
     }
     char goalsA[] = {'K','G','S','W','A'}; int nA = sizeof(goalsA)/sizeof(goalsA[0]);
     char goalsB[] = {'W','B','S','B','G','B','K','B'}; int nB = sizeof(goalsB)/sizeof(goalsB[0]);
     Point posA = coords['A'], posB = coords['B'], path[MAX_NODES];
     int dirA = 0, dirB = 0, len;
 
     // send initial positions
     fprintf(fa_w, "A:POS %d %d %d\n", posA.r, posA.c, dirA); fflush(fa_w);
     fprintf(fb_w, "B:POS %d %d %d\n", posB.r, posB.c, dirB); fflush(fb_w);
 
     // move A
     for (int idx = 0; idx < nA; idx++) {
         len = astar(grid, posA, coords[(unsigned char)goalsA[idx]], path);
         for (int i = 1; i < len; i++) {
            Point nxt = path[i];
            // 1) target_dir, diff 계산 (0=N,1=E,2=S,3=W)
            int td   = /* … */;
            int diff = (td - dirA + 4) % 4;
            if (diff == 3) diff = -1;   // 왼쪽 90° 한 칸
        
            // 2) diff 값만큼 90° 회전 명령을 반복
            if (diff > 0) {
                for (int k = 0; k < diff; k++) {
                    send_cmd(fa_w, fa_r, "A", "TURN_RIGHT", &posA, &dirA);
                }
            } else if (diff < 0) {
                for (int k = 0; k < -diff; k++) {
                    send_cmd(fa_w, fa_r, "A", "TURN_LEFT", &posA, &dirA);
                }
            }
        
            // 3) 한 칸 전진
            send_cmd(fa_w, fa_r, "A", "FORWARD", &posA, &dirA);
        }        
     }
     // move B
     for (int idx = 0; idx < nB; idx++) {
         len = astar(grid, posB, coords[(unsigned char)goalsB[idx]], path);
         for (int i = 1; i < len; i++) {
             Point nxt = path[i];
             int td = (nxt.r < posB.r ? 0 : (nxt.r > posB.r ? 2 : (nxt.c > posB.c ? 1 : 3)));
             int diff = (td - dirB + 4) % 4; if (diff == 3) diff = -1;
             if (diff > 0) send_cmd(fb_w, fb_r, "B", "TURN_RIGHT", &posB, &dirB);
             else if (diff < 0) send_cmd(fb_w, fb_r, "B", "TURN_LEFT", &posB, &dirB);
             send_cmd(fb_w, fb_r, "B", "FORWARD", &posB, &dirB);
         }
     }
     wait(NULL); wait(NULL);
     return 0;
 }
 