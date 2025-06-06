////나중에 가속도랑 쓸 코드


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <signal.h>
#include <sys/time.h>

#include <wiringPi.h>
#include <softPwm.h>
#include "imu_wrapper.h"  // RTIMULib2 C++ 래퍼 헤더

//=============== 모터 핀 정의 ================
#define AIN1 22
#define AIN2 27
#define PWMA 18

#define BIN1 25
#define BIN2 24
#define PWMB 23

//=============== 전역 <격자 위치·방향> ================
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

// 초기 위치 (0,0), 방향 NORTH
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

//=============== 상수: 시간 기반 전진 ================
#define TIME_PER_CM_US 100500UL  // 1 cm당 약 100.5 ms
#define FORWARD_DIST_CM 30.0f

//=============== 모터 제어 함수 ================
static void motor_go() {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    softPwmWrite(PWMA, 50);
    softPwmWrite(PWMB, 50);
}

static void motor_stop() {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
    softPwmWrite(PWMA, 0);
    softPwmWrite(PWMB, 0);
}

static void motor_right() {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
    softPwmWrite(PWMA, 50);
    softPwmWrite(PWMB, 50);
}

static void motor_left() {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    softPwmWrite(PWMA, 50);
    softPwmWrite(PWMB, 50);
}

//=============== SIGINT 핸들러 ================
static void handle_sigint(int sig) {
    printf("\n🛑 인터럽트 발생 - 모터 정지 후 종료합니다.\n");
    motor_stop();
    exit(0);
}

//=============== 회전: IMU 기반 90° 우회전 ================
static void rotate_right_90_imu() {
    printf("↩ 우회전 시작 (IMU 제어)\n");

    float initialYaw = imu_getYaw();  // IMU에서 현재 yaw(라디안) 읽기

    motor_right();

    const float TARGET_RAD = M_PI / 2.0f;                // 90°
    const float YAW_TOLERANCE = 1.5f * (M_PI / 180.0f);  // ±1.5°

    while (1) {
        float currentYaw = imu_getYaw();
        float deltaYaw = currentYaw - initialYaw;
        // [–π, +π] 범위로 wrap
        if (deltaYaw >  M_PI)   deltaYaw -= 2.0f * M_PI;
        if (deltaYaw < -M_PI)   deltaYaw += 2.0f * M_PI;

        // 우회전 방향은 deltaYaw가 양수로 증가
        if (deltaYaw >= (TARGET_RAD - YAW_TOLERANCE)) {
            break;
        }
        usleep(5000);
    }

    motor_stop();
    pos.dir = (pos.dir + 1) % 4;
    printf("📐 우회전 완료 (~90°). 방향: %s\n", dir_name(pos.dir));
}

//=============== 회전: IMU 기반 90° 좌회전 ================
static void rotate_left_90_imu() {
    printf("↪ 좌회전 시작 (IMU 제어)\n");

    float initialYaw = imu_getYaw();

    motor_left();

    const float TARGET_RAD = M_PI / 2.0f;
    const float YAW_TOLERANCE = 1.5f * (M_PI / 180.0f);

    while (1) {
        float currentYaw = imu_getYaw();
        float deltaYaw = currentYaw - initialYaw;
        if (deltaYaw >  M_PI)   deltaYaw -= 2.0f * M_PI;
        if (deltaYaw < -M_PI)   deltaYaw += 2.0f * M_PI;

        // 좌회전 방향은 deltaYaw가 음수로 내려감
        if (deltaYaw <= -(TARGET_RAD - YAW_TOLERANCE)) {
            break;
        }
        usleep(5000);
    }

    motor_stop();
    pos.dir = (pos.dir + 3) % 4;
    printf("📐 좌회전 완료 (~90°). 방향: %s\n", dir_name(pos.dir));
}

//=============== 전진: 시간 기반 제어 ================
static void move_forward_by_time(float target_cm) {
    unsigned long duration_us = (unsigned long)roundf(target_cm * TIME_PER_CM_US);
    printf("▶ 전진 시작: 목표 %.2f cm, 약 %lu μs 동안 전진\n", target_cm, duration_us);
    motor_go();
    usleep(duration_us);
    motor_stop();

    int grid_steps = (int)roundf(target_cm / 30.0f);
    switch (pos.dir) {
        case NORTH: pos.y += grid_steps; break;
        case EAST:  pos.x += grid_steps; break;
        case SOUTH: pos.y -= grid_steps; break;
        case WEST:  pos.x -= grid_steps; break;
    }
    printf("📍 전진 완료(시간 제어). 격자=(%d,%d), 방향=%s\n",
           pos.x, pos.y, dir_name(pos.dir));
}

//=============== 모터 셋업 ================
static void setup_motors() {
    wiringPiSetupGpio();
    pinMode(AIN1, OUTPUT);
    pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT);
    pinMode(BIN2, OUTPUT);
    softPwmCreate(PWMA, 0, 100);
    softPwmCreate(PWMB, 0, 100);
}

int main() {
    signal(SIGINT, handle_sigint);

    // 1) IMU 초기화 (C 래퍼를 통해 RTIMULib2 사용)
    imu_init();
    printf("[RTIMULib2] IMU 초기화 완료 (C 래퍼 via imu_init)\n");

    // 2) 모터 초기화
    setup_motors();

    //===============================================
    //  메인 시나리오:
    //   1) 30 cm 전진 → 2) 좌회전(IMU) → 3) 30 cm 전진 → 4) 우회전(IMU)
    //===============================================
    printf("\n=== 시간 기반 전진 + IMU 기반 회전 테스트 ===\n\n");

    // 1) 30cm 전진
    move_forward_by_time(FORWARD_DIST_CM);
    sleep(1);

    // 2) 90° 좌회전 (IMU)
    rotate_left_90_imu();
    sleep(1);

    // 3) 30cm 전진
    move_forward_by_time(FORWARD_DIST_CM);
    sleep(1);

    // 4) 90° 우회전 (IMU)
    rotate_right_90_imu();
    sleep(1);

    printf("\n🎯 최종 위치: (%d, %d), 방향: %s\n",
           pos.x, pos.y, dir_name(pos.dir));

    return 0;
}
