#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <signal.h>
#include <sys/time.h>
#include "mpu6050.h"

#define AIN1 22
#define AIN2 27
#define PWMA 18

#define BIN1 25
#define BIN2 24
#define PWMB 23
#define GYRO_NOISE_THRESHOLD 0.1f
#define ACCEL_NOISE_THRESHOLD 0.02f
#define MAX_FORWARD_TIME_US 3000000

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

void print_acceleration_status(int duration_ms)
{
    float ax, ay, az;
    int elapsed = 0;
    while (elapsed < duration_ms)
    {
        if (get_acceleration(&ax, &ay, &az) == 0)
        {
            float a = sqrt(ax * ax + ay * ay);
            if (a < ACCEL_NOISE_THRESHOLD)
                a = 0.0f;
            printf("📡 전진 중 가속도: a=%.3f (ax=%.3f, ay=%.3f)\n", a, ax, ay);
        }
        usleep(100000);
        elapsed += 100;
    }
}

float accumulate_angle_during_rotation(int duration_ms)
{
    float gx, gy, gz;
    float angle = 0.0f;
    struct timeval prev, now;
    gettimeofday(&prev, NULL);
    int elapsed = 0;
    while (elapsed < duration_ms)
    {
        gettimeofday(&now, NULL);
        float dt = (now.tv_sec - prev.tv_sec) + (now.tv_usec - prev.tv_usec) / 1000000.0f;
        prev = now;
        if (get_gyroscope(&gx, &gy, &gz) == 0)
        {
            if (fabs(gz) < GYRO_NOISE_THRESHOLD)
                gz = 0.0f;
            angle += gz * dt;
        }
        usleep(5000);
        elapsed += (int)(dt * 1000);
    }
    return angle;
}

void move_forward_cm(float cm, float time_per_cm_us)
{
    float duration_us = cm * time_per_cm_us;
    if (duration_us > MAX_FORWARD_TIME_US)
    {
        printf("⚠️ 이동 시간이 너무 김. 제한 적용됨 (%.0fμs → %dμs)\n", duration_us, MAX_FORWARD_TIME_US);
        duration_us = MAX_FORWARD_TIME_US;
    }
    motor_go();
    print_acceleration_status((int)(duration_us / 1000));
    usleep((int)duration_us);
    motor_stop();

    switch (pos.dir)
    {
    case NORTH:
        pos.y += round(cm / 30.0);
        break;
    case EAST:
        pos.x += round(cm / 30.0);
        break;
    case SOUTH:
        pos.y -= round(cm / 30.0);
        break;
    case WEST:
        pos.x -= round(cm / 30.0);
        break;
    }

    printf("📍 현재 위치: (%d, %d), 방향: %s\n", pos.x, pos.y, dir_name(pos.dir));
}

float average_speed_calibration(int trials)
{
    float average_time_per_cm = 100000.0f;
    printf("✅ 보정 생략: 20cm 이동 기준으로 %.1f μs/cm 사용\n", average_time_per_cm);
    return average_time_per_cm;
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

void motor_setup()
{
    wiringPiSetupGpio();
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
    softPwmWrite(PWMA, 50);
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

void rotate_right_90()
{
    motor_right();
    float angle = accumulate_angle_during_rotation(1500);
    motor_stop();
    rotate_right_dir();
    printf("📐 회전 누적 각도: %.2f도 (우)\n", angle);
}

void rotate_left_90()
{
    motor_left();
    float angle = accumulate_angle_during_rotation(1700);
    motor_stop();
    rotate_left_dir();
    printf("📐 회전 누적 각도: %.2f도 (좌)\n", angle);
}

void handle_sigint(int sig)
{
    printf("\n🛑 인터럽트 발생 - 모터 정지 후 종료합니다.\n");
    motor_stop();
    exit(0);
}

int main()
{
    signal(SIGINT, handle_sigint);

    if (mpu6050_init("/dev/i2c-1") < 0)
    {
        printf("MPU6050 초기화 실패\n");
        return 1;
    }

    motor_setup();

    float time_per_cm_us = average_speed_calibration(3);

    move_forward_cm(28.0f, time_per_cm_us);
    rotate_right_90();
    move_forward_cm(30.0f, time_per_cm_us);

    printf("🎯 최종 위치: (%d, %d), 방향: %s\n", pos.x, pos.y, dir_name(pos.dir));
    return 0;
}
