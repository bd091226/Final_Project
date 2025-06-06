#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/time.h>

#define MAX_PATH_LENGTH 100

typedef struct
{
    int x;
    int y;
} Position;

typedef enum
{
    N,
    E,
    S,
    W
} Direction;

const int DIR_VECTORS[4][2] = {
    {-1, 0}, // N
    {0, 1},  // E
    {1, 0},  // S
    {0, -1}  // W
};

// BCM GPIO 번호 정의 (libgpiod는 gpiochip0 기준 라인번호 = BCM 번호)
#define IN1_PIN 17
#define IN2_PIN 18
#define ENA_PIN 12
#define IN3_PIN 22
#define IN4_PIN 23
#define ENB_PIN 13

// 초음파 센서 핀 (예시, 필요시 변경)
#define TRIG_PIN 6
#define ECHO_PIN 5

struct gpiod_chip *chip;
struct gpiod_line *in1_line;
struct gpiod_line *in2_line;
struct gpiod_line *ena_line;
struct gpiod_line *in3_line;
struct gpiod_line *in4_line;
struct gpiod_line *enb_line;

struct gpiod_line *trig_line;
struct gpiod_line *echo_line;

void delay_ms(int ms)
{
    usleep(ms * 1000);
}

void pwm_set_duty(struct gpiod_line *line, int duty_percent)
{
    if (duty_percent > 0)
        gpiod_line_set_value(line, 1);
    else
        gpiod_line_set_value(line, 0);
}

void setup()
{
    chip = gpiod_chip_open_by_name("gpiochip0");
    if (!chip)
    {
        perror("gpiochip0 open failed");
        exit(1);
    }

    in1_line = gpiod_chip_get_line(chip, IN1_PIN);
    in2_line = gpiod_chip_get_line(chip, IN2_PIN);
    ena_line = gpiod_chip_get_line(chip, ENA_PIN);
    in3_line = gpiod_chip_get_line(chip, IN3_PIN);
    in4_line = gpiod_chip_get_line(chip, IN4_PIN);
    enb_line = gpiod_chip_get_line(chip, ENB_PIN);

    trig_line = gpiod_chip_get_line(chip, TRIG_PIN);
    echo_line = gpiod_chip_get_line(chip, ECHO_PIN);

    if (!in1_line || !in2_line || !ena_line || !in3_line || !in4_line || !enb_line || !trig_line || !echo_line)
    {
        fprintf(stderr, "GPIO line request failed\n");
        exit(1);
    }

    if (gpiod_line_request_output(in1_line, "motor_control", 0) < 0 ||
        gpiod_line_request_output(in2_line, "motor_control", 0) < 0 ||
        gpiod_line_request_output(ena_line, "motor_control", 0) < 0 ||
        gpiod_line_request_output(in3_line, "motor_control", 0) < 0 ||
        gpiod_line_request_output(in4_line, "motor_control", 0) < 0 ||
        gpiod_line_request_output(enb_line, "motor_control", 0) < 0 ||
        gpiod_line_request_output(trig_line, "ultrasonic_trig", 0) < 0)
    {
        fprintf(stderr, "GPIO output request failed\n");
        exit(1);
    }

    if (gpiod_line_request_input(echo_line, "ultrasonic_echo") < 0)
    {
        fprintf(stderr, "GPIO input request failed\n");
        exit(1);
    }
}

void cleanup()
{
    gpiod_line_release(in1_line);
    gpiod_line_release(in2_line);
    gpiod_line_release(ena_line);
    gpiod_line_release(in3_line);
    gpiod_line_release(in4_line);
    gpiod_line_release(enb_line);
    gpiod_line_release(trig_line);
    gpiod_line_release(echo_line);
    gpiod_chip_close(chip);
}

void set_speed(int speedA, int speedB)
{
    pwm_set_duty(ena_line, speedA);
    pwm_set_duty(enb_line, speedB);
}

void forward(int speed)
{
    gpiod_line_set_value(in1_line, 1);
    gpiod_line_set_value(in2_line, 0);
    gpiod_line_set_value(in3_line, 1);
    gpiod_line_set_value(in4_line, 0);
    set_speed(speed, speed);
}

void backward(int speed)
{
    gpiod_line_set_value(in1_line, 0);
    gpiod_line_set_value(in2_line, 1);
    gpiod_line_set_value(in3_line, 0);
    gpiod_line_set_value(in4_line, 1);
    set_speed(speed, speed);
}

void turn_left(int speed)
{
    gpiod_line_set_value(in1_line, 0);
    gpiod_line_set_value(in2_line, 1);
    gpiod_line_set_value(in3_line, 1);
    gpiod_line_set_value(in4_line, 0);
    set_speed(speed, speed);
}

void turn_right(int speed)
{
    gpiod_line_set_value(in1_line, 1);
    gpiod_line_set_value(in2_line, 0);
    gpiod_line_set_value(in3_line, 0);
    gpiod_line_set_value(in4_line, 1);
    set_speed(speed, speed);
}

void stop_motor()
{
    gpiod_line_set_value(in1_line, 0);
    gpiod_line_set_value(in2_line, 0);
    gpiod_line_set_value(in3_line, 0);
    gpiod_line_set_value(in4_line, 0);
    set_speed(0, 0);
}

// 현재 시각을 마이크로초 단위로 반환 (초음파 측정용)
unsigned long get_microseconds()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (unsigned long)(tv.tv_sec * 1000000 + tv.tv_usec);
}

// 초음파 센서 거리 측정 함수 (단위 cm)
float get_distance_cm()
{
    // 트리거 핀 LOW 유지 2us 이상
    gpiod_line_set_value(trig_line, 0);
    usleep(2);

    // 트리거 핀 HIGH 10us 펄스 출력
    gpiod_line_set_value(trig_line, 1);
    usleep(10);
    gpiod_line_set_value(trig_line, 0);

    // 에코 핀이 HIGH 되는 시간 측정
    unsigned long start_time = 0;
    unsigned long end_time = 0;

    // 에코 핀이 HIGH 되길 기다림 (timeout 200ms)
    unsigned long timeout = get_microseconds() + 200000;
    while (gpiod_line_get_value(echo_line) == 0)
    {
        if (get_microseconds() > timeout)
        {
            fprintf(stderr, "Echo pulse start timeout\n");
            return -1.0;
        }
    }
    start_time = get_microseconds();

    // 에코 핀이 LOW 될 때까지 대기 (timeout 200ms)
    timeout = get_microseconds() + 200000;
    while (gpiod_line_get_value(echo_line) == 1)
    {
        if (get_microseconds() > timeout)
        {
            fprintf(stderr, "Echo pulse end timeout\n");
            return -1.0;
        }
    }
    end_time = get_microseconds();

    // 펄스 길이(us)
    unsigned long pulse_duration = end_time - start_time;

    // 음속 34300 cm/s, 거리 = (시간 * 속도) / 2
    float distance = (pulse_duration * 0.0343) / 2.0;

    return distance;
}

Direction move_step(Position curr, Position next, Direction current_dir)
{
    int dx = next.x - curr.x;
    int dy = next.y - curr.y;

    int current_idx = current_dir;
    int target_idx = -1;
    for (int i = 0; i < 4; i++)
    {
        if (DIR_VECTORS[i][0] == dx && DIR_VECTORS[i][1] == dy)
        {
            target_idx = i;
            break;
        }
    }
    if (target_idx == -1)
    {
        printf("⚠️ 방향 계산 실패: dx=%d dy=%d\n", dx, dy);
        stop_motor();
        return current_dir;
    }

    int diff = (target_idx - current_idx + 4) % 4;

    switch (diff)
    {
    case 0:
        printf("⬆️ 직진\n");
        forward(40);
        break;
    case 1:
        printf("➡️ 우회전\n");
        turn_right(50);
        usleep(850 * 1000);
        forward(40);
        break;
    case 2:
        printf("🔄 유턴\n");
        turn_right(50);
        usleep(1700 * 1000);
        turn_right(50);
        usleep(1700 * 1000);
        forward(40);
        break;
    case 3:
        printf("⬅️ 좌회전\n");
        turn_left(40);
        usleep(550 * 1000);
        forward(40);
        break;
    default:
        printf("⚠️ 예외 상황\n");
        stop_motor();
        break;
    }

    usleep(700 * 1000);
    stop_motor();
    usleep(300 * 1000);

    return (Direction)target_idx;
}

int load_path_from_file(const char *filename, Position path[])
{
    FILE *fp = fopen(filename, "r");
    if (!fp)
    {
        printf("파일 열기 실패: %s\n", filename);
        return 0;
    }
    int count = 0;
    while (count < MAX_PATH_LENGTH && fscanf(fp, "%d %d", &path[count].x, &path[count].y) == 2)
    {
        count++;
    }
    fclose(fp);
    return count;
}

int main()
{
    setup();

    Position path[MAX_PATH_LENGTH];
    int path_len = load_path_from_file("path_S_to_A.txt", path);
    Direction current_dir = E;

    if (path_len <= 0)
    {
        fprintf(stderr, "경로 파일 읽기 실패\n");
        cleanup();
        return 1;
    }

    printf("[차량 이동 시작: S → A]\n");

    for (int i = 0; i < path_len - 1; i++)
    {
        while (1)
        {
            float distance = get_distance_cm();
            if (distance < 0)
            {
                fprintf(stderr, "거리 측정 실패\n");
                stop_motor();
                break;
            }
            printf("거리: %.2f cm\n", distance);

            if (distance < 10.0)
            {
                printf("거리 10cm 이하 - 차량 정지\n");
                stop_motor();
                delay_ms(100);
            }
            else
            {
                printf("이동 재개\n");
                current_dir = move_step(path[i], path[i + 1], current_dir);
                break; // 다음 위치로 진행
            }
        }
    }

    turn_left(40);
    usleep(550 * 1000);

    stop_motor();
    cleanup();

    return 0;
}
