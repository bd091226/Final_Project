// testencoder.c
// 전진 및 회전을 forward_one, rotate_one 기반으로 동작
// 스레드 대신 각 모션 함수 내에서 동기적으로 엔코더 이벤트 처리
// gcc -g testencoder.c -o testencoder -lgpiod

#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <sys/poll.h>

#include <time.h>
#include <math.h>
#include <stdbool.h>

#define CHIPNAME "gpiochip0"
#define SEC_TO_US(sec) ((useconds_t)((sec) * 1e6))
#define SECONDS_PER_GRID_STEP 2.0
#define SECONDS_PER_90_DEG_ROTATION 0.73
#define PRE_ROTATE_FORWARD_CM 6.0
#define SPEED 100

#define IN1   17
#define IN2   18
#define ENA   12
#define IN3   22
#define IN4   23
#define ENB   13
#define ENC_A 14
#define ENC_B 15

#define NORTH 0
#define EAST  1
#define SOUTH 2
#define WEST  3

volatile int countA = 0, countB = 0;
volatile int running = 1;

struct Point { int r, c; };

struct gpiod_chip *chip;
struct gpiod_line *in1, *in2, *ena, *in3, *in4, *enb;
struct gpiod_line *encA, *encB;

// 제어 및 유틸 함수
void motor_stop();
long get_microseconds();
void safe_set_value(struct gpiod_line *line, int value, const char* name);
void cleanup_and_exit();
void handle_sigint(int sig);
void print_counts(const char* tag);
void reset_counts();

// 각 모션에서 동기적으로 엔코더 읽기
void forward_one(struct Point *pos, int dir) {
    printf("➡️ forward_one at (%d,%d) dir=%d\n", pos->r, pos->c, dir);
    // 모터 전진 ON
    safe_set_value(in1, 0, "IN1");
    safe_set_value(in2, 1, "IN2");
    safe_set_value(in3, 0, "IN3");
    safe_set_value(in4, 1, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");

    // 엔코더 폴링
    struct pollfd pfds[2] = {{gpiod_line_event_get_fd(encA), POLLIN}, {gpiod_line_event_get_fd(encB), POLLIN}};
    struct gpiod_line_event event;
    long end_time = get_microseconds() + SEC_TO_US(SECONDS_PER_GRID_STEP);

    while (get_microseconds() < end_time) {
        if (poll(pfds, 2, 100) > 0) {
            if (pfds[0].revents & POLLIN) {
                gpiod_line_event_read(encA, &event);
                countA++;
            }
            if (pfds[1].revents & POLLIN) {
                gpiod_line_event_read(encB, &event);
                countB++;
            }
        }
    }
    // 모터 정지
    motor_stop();

    // 위치 업데이트
    switch (dir) {
        case NORTH: pos->r--; break;
        case EAST:  pos->c++; break;
        case SOUTH: pos->r++; break;
        case WEST:  pos->c--; break;
    }
}

void backward_one(struct Point *pos, int dir) {
    printf("⬅️ backward_one at (%d,%d) dir=%d\n", pos->r, pos->c, dir);
    // 모터 후진 ON
    safe_set_value(in1, 1, "IN1");
    safe_set_value(in2, 0, "IN2");
    safe_set_value(in3, 1, "IN3");
    safe_set_value(in4, 0, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");

    // 엔코더 폴링
    struct pollfd pfds[2] = {{gpiod_line_event_get_fd(encA), POLLIN}, {gpiod_line_event_get_fd(encB), POLLIN}};
    struct gpiod_line_event event;
    long end_time = get_microseconds() + SEC_TO_US(SECONDS_PER_GRID_STEP);

    while (get_microseconds() < end_time) {
        if (poll(pfds, 2, 100) > 0) {
            if (pfds[0].revents & POLLIN) { gpiod_line_event_read(encA, &event); countA++; }
            if (pfds[1].revents & POLLIN) { gpiod_line_event_read(encB, &event); countB++; }
        }
    }
    motor_stop();

    switch (dir) {
        case NORTH: pos->r++; break;
        case EAST:  pos->c--; break;
        case SOUTH: pos->r--; break;
        case WEST:  pos->c++; break;
    }
}

void rotate_one(int *dir, int turn_dir) {
    printf("🔄 rotate_one dir->%d + %d\n", *dir, turn_dir);
    // 사전 전진
    long prep = SEC_TO_US((PRE_ROTATE_FORWARD_CM/30.0)*SECONDS_PER_GRID_STEP);
    struct pollfd prep_pfds[2] = {{gpiod_line_event_get_fd(encA), POLLIN}, {gpiod_line_event_get_fd(encB), POLLIN}};
    struct gpiod_line_event evt;
    safe_set_value(in1, 0, "IN1"); safe_set_value(in2, 1, "IN2");
    safe_set_value(in3, 0, "IN3"); safe_set_value(in4, 1, "IN4");
    safe_set_value(ena, 1, "ENA"); safe_set_value(enb, 1, "ENB");
    long prep_end = get_microseconds() + prep;
    while (get_microseconds() < prep_end) {
        poll(prep_pfds, 2, 100);
    }

    // 90도 회전
    safe_set_value(ena, 0, "ENA"); safe_set_value(enb, 0, "ENB");
    usleep(100000);
    if (turn_dir>0) {
        // CW
        safe_set_value(in1, 0, "IN1"); safe_set_value(in2, 1, "IN2");
        safe_set_value(in3, 1, "IN3"); safe_set_value(in4, 0, "IN4");
    } else {
        // CCW
        safe_set_value(in1, 1, "IN1"); safe_set_value(in2, 0, "IN2");
        safe_set_value(in3, 0, "IN3"); safe_set_value(in4, 1, "IN4");
    }
    safe_set_value(ena, 1, "ENA"); safe_set_value(enb, 1, "ENB");
    long rot_end = get_microseconds() + SEC_TO_US(SECONDS_PER_90_DEG_ROTATION);
    while (get_microseconds() < rot_end) { poll(prep_pfds,2,100); }
    motor_stop();

    *dir = (*dir + turn_dir + 4) % 4;
}

// 기타 유틸
long get_microseconds() {
    struct timespec ts; clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec*1000000L + ts.tv_nsec/1000L;
}

void motor_stop() {
    safe_set_value(in1,0,"IN1"); safe_set_value(in2,0,"IN2");
    safe_set_value(in3,0,"IN3"); safe_set_value(in4,0,"IN4");
    safe_set_value(ena,0,"ENA"); safe_set_value(enb,0,"ENB");
}

void safe_set_value(struct gpiod_line *line,int v,const char*name){ if(!line){cleanup_and_exit();} gpiod_line_set_value(line,v); }
void print_counts(const char*t){ printf("[%s] A:%d B:%d\n",t,countA,countB); }
void reset_counts(){countA=countB=0;}
void handle_sigint(int sig){(void)sig;running=0;cleanup_and_exit();}
void cleanup_and_exit(){ motor_stop(); gpiod_chip_close(chip); exit(0);}  

int main() {
    signal(SIGINT,handle_sigint);
    chip = gpiod_chip_open_by_name(CHIPNAME);
    in1=gpiod_chip_get_line(chip,IN1);
    in2=gpiod_chip_get_line(chip,IN2);
    ena=gpiod_chip_get_line(chip,ENA);
    in3=gpiod_chip_get_line(chip,IN3);
    in4=gpiod_chip_get_line(chip,IN4);
    enb=gpiod_chip_get_line(chip,ENB);
    encA=gpiod_chip_get_line(chip,ENC_A);
    encB=gpiod_chip_get_line(chip,ENC_B);
    if(!in1||!in2||!ena||!in3||!in4||!enb||!encA||!encB) cleanup_and_exit();
    gpiod_line_request_output(in1,"motor",0);
    gpiod_line_request_output(in2,"motor",0);
    gpiod_line_request_output(ena,"motor",0);
    gpiod_line_request_output(in3,"motor",0);
    gpiod_line_request_output(in4,"motor",0);
    gpiod_line_request_output(enb,"motor",0);
    gpiod_line_request_both_edges_events_flags(encA,"encA",GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
    gpiod_line_request_both_edges_events_flags(encB,"encB",GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);

    struct Point pos={0,0}; int dir=NORTH;
    char cmd[32];
    printf("[명령] G/B/L/R/S/E\n");
    while(running) {
        printf(">>> ");
        if(!fgets(cmd,sizeof(cmd),stdin)) break;
        char c=toupper(cmd[0]);
        reset_counts();
        switch(c){
            case 'G': forward_one(&pos,dir); print_counts("GO"); break;
            case 'B': backward_one(&pos,dir); print_counts("BACK"); break;
            case 'L': rotate_one(&dir,-1); print_counts("LEFT"); break;
            case 'R': rotate_one(&dir,1); print_counts("RIGHT"); break;
            case 'S': motor_stop(); print_counts("STOP"); break;
            case 'E': running=0; break;
            default: printf("[오류]\n");
        }
    }
    cleanup_and_exit();
    return 0;
}
