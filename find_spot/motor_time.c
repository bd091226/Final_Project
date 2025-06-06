// 컴파일:
//   gcc motor_time.c -o motor_time -lwiringPi -lm
//
// 실행:
//   ./motor_time

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>

#include <wiringPi.h>
#include <softPwm.h>

#define AIN1 22
#define AIN2 27
#define PWMA 18

#define BIN1 25
#define BIN2 24
#define PWMB 23

typedef enum {
    NORTH = 0,
    EAST  = 1,
    SOUTH = 2,
    WEST  = 3
} Direction;

typedef struct {
    int x;
    int y;
    Direction dir;
} Position;

static Position pos = {0, 0, NORTH};

static const char *dir_name(Direction d) {
    switch (d) {
        case NORTH: return "NORTH";
        case EAST:  return "EAST";
        case SOUTH: return "SOUTH";
        case WEST:  return "WEST";
        default:    return "UNKNOWN";
    }
}

#define SECONDS_PER_GRID_STEP 2.2

#define SECONDS_PER_90_DEG_ROTATION 1.24

#define FORWARD_STEPS 1   // 격자 단위 전진 수
#define ROTATE_LEFT   1   // 90° 회전 수 (반시계)
#define ROTATE_RIGHT  1   // 90° 회전 수 (시계)

static void motor_go(void) {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    softPwmWrite(PWMA, 60);
    softPwmWrite(PWMB, 60);
}

static void motor_stop(void) {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
    softPwmWrite(PWMA, 0);
    softPwmWrite(PWMB, 0);
}

static void motor_right(void) {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
    softPwmWrite(PWMA, 60);
    softPwmWrite(PWMB, 60);
}

static void motor_left(void) {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    softPwmWrite(PWMA, 60);
    softPwmWrite(PWMB, 60);
}

static void handle_sigint(int sig) {
    (void)sig;
    printf("\n🛑 인터럽트 발생 - 모터 정지 후 종료합니다.\n");
    motor_stop();
    exit(0);
}

static void move_forward_by_time(int steps) {
    if (steps <= 0) return;

    double total_seconds = steps * SECONDS_PER_GRID_STEP;
    unsigned int micro = (unsigned int)(total_seconds * 1e6);  // μs 단위

    printf("▶ 전진 시작: %d 격자 (약 %.2f초)\n", steps, total_seconds);
    motor_go();
    usleep(micro);
    motor_stop();

    switch (pos.dir) {
        case NORTH: pos.y += steps; break;
        case EAST:  pos.x += steps; break;
        case SOUTH: pos.y -= steps; break;
        case WEST:  pos.x -= steps; break;
    }
    printf("📍 전진 완료. 현재 위치 = (%d, %d), 방향 = %s\n",
           pos.x, pos.y, dir_name(pos.dir));
}

static void rotate_by_time(int num90, int direction) {
    if (num90 <= 0) return;

    double total_seconds = num90 * SECONDS_PER_90_DEG_ROTATION;
    unsigned int micro = (unsigned int)(total_seconds * 1e6);  // μs 단위

    if (direction > 0) {
        printf("↩ 우회전 시작: %d × 90° (약 %.2f초)\n", num90, total_seconds);
        motor_right();
    } else {
        printf("↪ 좌회전 시작: %d × 90° (약 %.2f초)\n", num90, total_seconds);
        motor_left();
    }

    usleep(micro);
    motor_stop();

    if (direction > 0) {
        pos.dir = (pos.dir + num90) % 4;
    } else {
        int d = (int)pos.dir - num90;
        while (d < 0) d += 4;
        pos.dir = (Direction)(d % 4);
    }
    printf("📐 회전 완료. 현재 방향 = %s\n", dir_name(pos.dir));
}

int main(void) {
    signal(SIGINT, handle_sigint);

    wiringPiSetupGpio();
    pinMode(AIN1, OUTPUT);
    pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT);
    pinMode(BIN2, OUTPUT);
    softPwmCreate(PWMA, 0, 100);
    softPwmCreate(PWMB, 0, 100);

    printf("=== 시간 기반 모터 제어 테스트 (PWM=60) ===\n\n");

    move_forward_by_time(FORWARD_STEPS);
    sleep(1);

    rotate_by_time(ROTATE_LEFT, -1);
    sleep(1);

    move_forward_by_time(FORWARD_STEPS);
    sleep(1);

    rotate_by_time(ROTATE_RIGHT, +1);
    sleep(1);

    printf("\n🎯 최종 위치: (%d, %d), 방향: %s\n",
           pos.x, pos.y, dir_name(pos.dir));

    return 0;
}
