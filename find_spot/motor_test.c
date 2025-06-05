#include <stdio.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <wiringPi.h>
#include <softPwm.h>
#include <signal.h>
#include <sys/time.h>
#include "mpu6050.h"
#include <stdlib.h>

#define AIN1 22
#define AIN2 27
#define PWMA 18

#define BIN1 25
#define BIN2 24
#define PWMB 23
#define GYRO_NOISE_THRESHOLD 0.1f
#define ACCEL_NOISE_THRESHOLD 0.02f
#define MAX_FORWARD_TIME_US 3000000

void motor_go();
void motor_stop();

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

void get_average_acceleration(int duration_ms, float* ax_out, float* ay_out, float* az_out)
{
    float ax, ay, az;
    float sum_ax = 0.0f, sum_ay = 0.0f, sum_az = 0.0f;
    int count = 0;
    int elapsed = 0;

    while (elapsed < duration_ms)
    {
        if (get_acceleration(&ax, &ay, &az) == 0)
        {
            printf("📡 전진 중 가속도: ax=%.3f, ay=%.3f, az=%.3f\n", ax, ay, az);
            sum_ax += ax;
            sum_ay += ay;
            sum_az += az;
            count++;
        }
        usleep(100000); // 100ms
        elapsed += 100;
    }

    if (count > 0)
    {
        *ax_out = sum_ax / count;
        *ay_out = sum_ay / count;
        *az_out = sum_az / count;
    }
    else
    {
        *ax_out = 0.0f;
        *ay_out = 0.0f;
        *az_out = 0.0f;
        printf("⚠️ 센서 측정 실패 - 평균 계산 안됨\n");
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

void move_forward_by_acceleration(float target_cm)
{
    float ax, ay, az;
    float vx = 0.0f, vy = 0.0f;
    float x = 0.0f, y = 0.0f;

    int log_interval = 100; // 100ms
    int time_since_log = 0;

    struct timeval prev, now;
    gettimeofday(&prev, NULL);

    get_average_acceleration(500, &ax, &ay, &az);  // 🔁 최초 평균값 측정
    ax *= 9.81f;  // G → m/s²
    ay *= 9.81f;

    vx += ax * 0.1f;  // 초깃값 보정 (선택)
    vy += ay * 0.1f;

    motor_go();
    usleep(100000);
    gettimeofday(&now, NULL);
    while (1)
    {
        float dt = (now.tv_sec - prev.tv_sec) + (now.tv_usec - prev.tv_usec) / 1000000.0f;
        prev = now;

        if (get_acceleration(&ax, &ay, &az) == 0)
        {
            // 노이즈 제거
            if (fabs(ax) < ACCEL_NOISE_THRESHOLD) ax = 0.0f;
            if (fabs(ay) < ACCEL_NOISE_THRESHOLD) ay = 0.0f;

            // 속도 적분
            vx += ax * dt;
            vy += ay * dt;

            // 거리 적분
            x += vx * dt;
            y += vy * dt;

            float distance_cm = sqrt(x * x + y * y) * 100.0f; // m → cm

            if (time_since_log >= log_interval)
            {
                printf("📏 누적 거리: %.2fcm, 속도 vx=%.3f, vy=%.3f\n", distance_cm, vx, vy);
                time_since_log = 0;
            }

            if (distance_cm >= target_cm)
                break;
        }

        usleep(5000); // 5ms 대기
        time_since_log += (int)(dt * 1000);
    }

    // 위치 갱신
    switch (pos.dir)
    {
    case NORTH: pos.y += round(target_cm / 30.0); break;
    case EAST:  pos.x += round(target_cm / 30.0); break;
    case SOUTH: pos.y -= round(target_cm / 30.0); break;
    case WEST:  pos.x -= round(target_cm / 30.0); break;
    }
    get_acceleration(&ax, &ay, &az);
    printf("📍 최종 위치: (%d, %d), 방향: %s\n", pos.x, pos.y, dir_name(pos.dir));
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
    move_forward_by_acceleration(30.0f);
    usleep(3000000);
    motor_stop();
    // // usleep(500000); // 500ms = 0.5초
    // // rotate_left_90();
    // // motor_stop();
    // // usleep(500000); // 500ms = 0.5초
    // // move_forward_by_acceleration(30.0f);

    // printf("🎯 최종 위치: (%d, %d), 방향: %s\n", pos.x, pos.y, dir_name(pos.dir));
    // return 0;
    

    ///////////////////////////////////////////////////////////////
    // go 함수 되는지 확인용 코드
    // motor_setup();
    // motor_go();
    // usleep(3000000);
    // motor_stop();
}
