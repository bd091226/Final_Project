#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <signal.h>
#include "mpu6050.h"

// 모터 핀 (BCM 기준)
#define AIN1 22
#define AIN2 27
#define PWMA 18

#define BIN1 25
#define BIN2 24
#define PWMB 23

// ===== 방향 및 좌표 =====
typedef enum
{
    NORTH = 0,
    EAST = 1,
    SOUTH = 2,
    WEST = 3
} Direction;

typedef struct
{
    int x;
    int y;
    Direction dir;
} Position;

Position pos = {0, 0, NORTH};

const char *dir_name(Direction d)
{
    switch (d)
    {
    case NORTH:
        return "NORTH";
    case EAST:
        return "EAST";
    case SOUTH:
        return "SOUTH";
    case WEST:
        return "WEST";
    default:
        return "UNKNOWN";
    }
}

void update_position()
{
    switch (pos.dir)
    {
    case NORTH:
        pos.y += 1;
        break;
    case EAST:
        pos.x += 1;
        break;
    case SOUTH:
        pos.y -= 1;
        break;
    case WEST:
        pos.x -= 1;
        break;
    }
    printf("📍 현재 위치: (%d, %d), 방향: %s\n", pos.x, pos.y, dir_name(pos.dir));
}

void rotate_right_dir()
{
    pos.dir = (pos.dir + 1) % 4;
    printf("↩ 방향 전환: %s\n", dir_name(pos.dir));
}

void rotate_left_dir()
{
    pos.dir = (pos.dir + 3) % 4;
    printf("↪ 방향 전환: %s\n", dir_name(pos.dir));
}

// ===== 실제 모터 제어 함수 (wiringPi 사용) =====
void motor_setup()
{
    wiringPiSetupGpio(); // BCM 번호 사용
    pinMode(AIN1, OUTPUT);
    pinMode(AIN2, OUTPUT);
    pinMode(BIN1, OUTPUT);
    pinMode(BIN2, OUTPUT);

    softPwmCreate(PWMA, 0, 100);
    softPwmCreate(PWMB, 0, 100);
}

void motor_go()
{
    printf("➡ 전진\n");
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    softPwmWrite(PWMA, 50); // 50% duty
    softPwmWrite(PWMB, 50);
}

void motor_stop()
{
    printf("■ 정지\n");
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, LOW);
    softPwmWrite(PWMA, 0);
    softPwmWrite(PWMB, 0);
}

void motor_right()
{
    printf("↩ 우회전\n");
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
    digitalWrite(BIN1, HIGH);
    digitalWrite(BIN2, LOW);
    softPwmWrite(PWMA, 50);
    softPwmWrite(PWMB, 50);
}

void motor_left()
{
    printf("↪ 좌회전\n");
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
    digitalWrite(BIN1, LOW);
    digitalWrite(BIN2, HIGH);
    softPwmWrite(PWMA, 50);
    softPwmWrite(PWMB, 50);
}

// ===== 동작 함수 =====
void move_forward_one_cell()
{
    float ax, ay, az;
    float dist = 0.0f;
    int time_ms = 0;
    int accel_count = 0;

    motor_go();

    while (dist < 30.0f && time_ms < 3000) // 최대 3초 동작 제한
    {
        if (get_acceleration(&ax, &ay, &az) == 0)
        {
            float a = fabs(ax);

            if (a > 0.1f) // 유효 가속도 조건
            {
                accel_count++;
                dist += 2.0f; // 1회 감지당 약 2cm 누적
            }

            printf("📡 ax = %.3f → 누적 거리: %.2f cm (감지 %d회)\n", a, dist, accel_count);
        }

        usleep(100000); // 100ms
        time_ms += 100;
    }

    motor_stop();
    update_position();
}

void rotate_right_90()
{
    float gx, gy, gz;
    float angle = 0;

    motor_right();
    while (angle < 90.0f)
    {
        if (get_gyroscope(&gx, &gy, &gz) == 0)
        {
            angle += fabs(gz * 0.1);
        }
        usleep(100000);
    }
    motor_stop();
    rotate_right_dir();
}

void rotate_left_90()
{
    float gx, gy, gz;
    float angle = 0;

    motor_left();
    while (angle < 90.0f)
    {
        if (get_gyroscope(&gx, &gy, &gz) == 0)
        {
            angle += fabs(gz * 0.1);
        }
        usleep(100000);
    }
    motor_stop();
    rotate_left_dir();
}

// ===== SIGINT 핸들러 추가 =====
void handle_sigint(int sig)
{
    printf("\n🛑 인터럽트 발생 - 모터 정지 후 종료합니다.\n");
    motor_stop();
    exit(0);
}

// ===== main() =====
int main()
{
    signal(SIGINT, handle_sigint);

    if (mpu6050_init("/dev/i2c-1") < 0)
    {
        printf("MPU6050 초기화 실패\n");
        return 1;
    }

    motor_setup();

    printf("✅ 시작 좌표: (%d, %d), 방향: %s\n", pos.x, pos.y, dir_name(pos.dir));

    move_forward_one_cell(); // (0,1)
    // rotate_right_90();       // EAST
    // move_forward_one_cell(); // (1,1)
    // rotate_left_90();        // NORTH
    // move_forward_one_cell(); // (1,2)

    printf("🎯 최종 위치: (%d, %d), 방향: %s\n", pos.x, pos.y, dir_name(pos.dir));
    return 0;
}
