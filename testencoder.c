// testencoder.c
// 전진 및 회전을 forward_one, rotate_one 기반으로 동작
// gcc -g testencoder.c -o testencoder -lgpiod -lpthread

#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <pthread.h>
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

#define IN1 17
#define IN2 18
#define ENA 12
#define IN3 22
#define IN4 23
#define ENB 13
#define ENC_A 14
#define ENC_B 15

#define NORTH 0
#define EAST  1
#define SOUTH 2
#define WEST  3

volatile int countA = 0, countB = 0;
volatile int running = 1;

struct Point {
    int r, c;
};

struct gpiod_chip *chip;
struct gpiod_line *in1, *in2, *ena, *in3, *in4, *enb;
struct gpiod_line *encA, *encB;
int fdA, fdB;

pthread_mutex_t count_lock = PTHREAD_MUTEX_INITIALIZER;

void motor_stop();

const int TRIG_PIN = 5;
const int ECHO_PIN = 9;

extern struct gpiod_chip *chip;

long get_microseconds()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
}

// 초음파 센서 핀 초기화 함수
void init_ultrasonic_pins(struct gpiod_chip *chip) {
    struct gpiod_line *trig = gpiod_chip_get_line(chip, TRIG_PIN);
    struct gpiod_line *echo = gpiod_chip_get_line(chip, ECHO_PIN);

    if (!trig || !echo) {
        perror("gpiod_chip_get_line (ultrasonic)");
        exit(EXIT_FAILURE);
    }

    // Trig 핀 출력 설정 (필수)
    if (gpiod_line_request_output(trig, "ultrasonic_trig", 0) < 0) {
        perror("gpiod_line_request_output (TRIG)");
        exit(EXIT_FAILURE);
    }

    // Echo 핀 입력 설정 (필수)
    if (gpiod_line_request_input(echo, "ultrasonic_echo") < 0) {
        perror("gpiod_line_request_input (ECHO)");
        exit(EXIT_FAILURE);
    }

    // 핀 해제
    gpiod_line_release(trig);
    gpiod_line_release(echo);
}

float measure_distance(struct gpiod_chip *chip)
{
    struct gpiod_line *trig = gpiod_chip_get_line(chip, TRIG_PIN);
    struct gpiod_line *echo = gpiod_chip_get_line(chip, ECHO_PIN);

    gpiod_line_request_output(trig, "trig", 0);
    gpiod_line_request_input(echo, "echo");

    // 트리거 펄스
    gpiod_line_set_value(trig, 0);
    usleep(2);
    gpiod_line_set_value(trig, 1);
    usleep(10);
    gpiod_line_set_value(trig, 0);

    long start_time = 0, end_time = 0, current_time;

    // Echo HIGH 대기 (타임아웃 30ms)
    long timeout = get_microseconds() + 30000;
    while (1) {
        current_time = get_microseconds();
        if (current_time > timeout) {
            gpiod_line_release(trig);
            gpiod_line_release(echo);
            return -1;  // 타임아웃 시 -1 반환
        }
        if (gpiod_line_get_value(echo) == 1) {
            start_time = current_time;
            break;
        }
    }

    // Echo LOW 대기 (타임아웃 30ms)
    timeout = get_microseconds() + 30000;
    while (1) {
        current_time = get_microseconds();
        if (current_time > timeout) {
            gpiod_line_release(trig);
            gpiod_line_release(echo);
            return -1;  // 타임아웃 시 -1 반환
        }
        if (gpiod_line_get_value(echo) == 0) {
            end_time = current_time;
            break;
        }
    }

    gpiod_line_release(trig);
    gpiod_line_release(echo);

    float dist = (end_time - start_time) * 0.0343 / 2.0;
    return dist;
}

bool check_obstacle(struct gpiod_chip *chip)
{
    float distance = measure_distance(chip);

    if (distance < 0)
    {
        return false; // 타임아웃 시 장애물로 인식하지 않음
    }

    if (distance <= 8.0 && distance > 0.1)
    {
        printf(" 장애물 감지! 이동 중지! 거리: %.2f cm\n", distance);
        return true;
    }
    
    return false; // 장애물이 없으면 false 반환
}

void cleanup_and_exit() {
    motor_stop();
    if (ena) gpiod_line_release(ena);
    if (enb) gpiod_line_release(enb);
    if (in1) gpiod_line_release(in1);
    if (in2) gpiod_line_release(in2);
    if (in3) gpiod_line_release(in3);
    if (in4) gpiod_line_release(in4);
    if (encA) gpiod_line_release(encA);
    if (encB) gpiod_line_release(encB);
    if (chip) gpiod_chip_close(chip);
    printf("\n[정리] 종료\n");
    exit(0);
}

void handle_sigint(int sig) {
    (void)sig;
    running = 0;
    cleanup_and_exit();
}

void safe_set_value(struct gpiod_line *line, int value, const char* name) {
    if (!line) {
        fprintf(stderr, "[NULL 라인] %s\n", name);
        cleanup_and_exit();
    }
    if (gpiod_line_set_value(line, value) < 0) {
        fprintf(stderr, "[에러] %s 값 설정 실패\n", name);
        cleanup_and_exit();
    }
}

void motor_stop() {
    safe_set_value(in1, 0, "IN1");
    safe_set_value(in2, 0, "IN2");
    safe_set_value(in3, 0, "IN3");
    safe_set_value(in4, 0, "IN4");
    safe_set_value(ena, 0, "ENA");
    safe_set_value(enb, 0, "ENB");
}

void motor_go_precise(struct gpiod_chip *chip, int speed, double duration) {
    (void)speed;  // 현재 하드웨어에서는 속도 파라미터 미사용
    safe_set_value(in1, 0, "IN1");
    safe_set_value(in2, 1, "IN2");
    safe_set_value(in3, 0, "IN3");
    safe_set_value(in4, 1, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");
    usleep(SEC_TO_US(duration));
    motor_stop();
}

void motor_left(int speed, double duration) {
    (void)speed;
    safe_set_value(in1, 1, "IN1");
    safe_set_value(in2, 0, "IN2");
    safe_set_value(in3, 0, "IN3");
    safe_set_value(in4, 1, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");
    usleep(SEC_TO_US(duration));
    motor_stop();
}

void motor_right(int speed, double duration) {
    (void)speed;
    safe_set_value(in1, 0, "IN1");
    safe_set_value(in2, 1, "IN2");
    safe_set_value(in3, 1, "IN3");
    safe_set_value(in4, 0, "IN4");
    safe_set_value(ena, 1, "ENA");
    safe_set_value(enb, 1, "ENB");
    usleep(SEC_TO_US(duration));
    motor_stop();
}

void forward_one(struct Point *pos, int dir, int speed) {
    printf("➡️ forward_one called at (%d,%d) dir=%d\n", pos->r, pos->c, dir);
    motor_go_precise(chip, speed, SECONDS_PER_GRID_STEP);
    switch (dir) {
        case NORTH: pos->r--; break;
        case EAST:  pos->c++; break;
        case SOUTH: pos->r++; break;
        case WEST:  pos->c--; break;
    }
}

void rotate_one(int *dir, int turn_dir, int speed) {
    double t0 = (PRE_ROTATE_FORWARD_CM / 30.0f) * SECONDS_PER_GRID_STEP;
    motor_go_precise(chip, speed, t0);
    usleep(100000);
    if (turn_dir > 0)
        motor_right(speed, SECONDS_PER_90_DEG_ROTATION);
    else
        motor_left(speed, SECONDS_PER_90_DEG_ROTATION);
    *dir = (*dir + turn_dir + 4) % 4;
}

void *encoder_thread(void *arg) {
    struct pollfd pfds[2] = {
        { .fd = fdA, .events = POLLIN },
        { .fd = fdB, .events = POLLIN }
    };
    struct gpiod_line_event event;

    while (running) {
        int ret = poll(pfds, 2, 100);
        if (ret > 0) {
            if (pfds[0].revents & POLLIN) {
                gpiod_line_event_read(encA, &event);
                pthread_mutex_lock(&count_lock);
                countA++;
                pthread_mutex_unlock(&count_lock);
            }
            if (pfds[1].revents & POLLIN) {
                gpiod_line_event_read(encB, &event);
                pthread_mutex_lock(&count_lock);
                countB++;
                pthread_mutex_unlock(&count_lock);
            }
        }
    }

    return NULL;
}

void print_counts(const char* tag) {
    pthread_mutex_lock(&count_lock);
    printf("[%s] 엔코더 A: %d, B: %d\n", tag, countA, countB);
    pthread_mutex_unlock(&count_lock);
}

void reset_counts() {
    pthread_mutex_lock(&count_lock);
    countA = 0; countB = 0;
    pthread_mutex_unlock(&count_lock);
}

int main() {
    signal(SIGINT, handle_sigint);

    chip = gpiod_chip_open_by_name(CHIPNAME);
    if (!chip) { perror("gpiod_chip_open_by_name 실패"); return 1; }

    in1 = gpiod_chip_get_line(chip, IN1);
    in2 = gpiod_chip_get_line(chip, IN2);
    ena = gpiod_chip_get_line(chip, ENA);
    in3 = gpiod_chip_get_line(chip, IN3);
    in4 = gpiod_chip_get_line(chip, IN4);
    enb = gpiod_chip_get_line(chip, ENB);
    encA = gpiod_chip_get_line(chip, ENC_A);
    encB = gpiod_chip_get_line(chip, ENC_B);

    if (!in1 || !in2 || !ena || !in3 || !in4 || !enb || !encA || !encB) {
        fprintf(stderr, "[에러] GPIO 라인 초기화 실패\n");
        cleanup_and_exit();
    }

    if (gpiod_line_request_output(in1, "motor", 0) < 0 ||
        gpiod_line_request_output(in2, "motor", 0) < 0 ||
        gpiod_line_request_output(ena, "motor", 0) < 0 ||
        gpiod_line_request_output(in3, "motor", 0) < 0 ||
        gpiod_line_request_output(in4, "motor", 0) < 0 ||
        gpiod_line_request_output(enb, "motor", 0) < 0) {
        fprintf(stderr, "[에러] 출력 요청 실패\n");
        cleanup_and_exit();
    }

    if (gpiod_line_request_both_edges_events_flags(encA, "encA", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP) < 0 ||
        gpiod_line_request_both_edges_events_flags(encB, "encB", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP) < 0) {
        fprintf(stderr, "[에러] 엔코더 이벤트 요청 실패\n");
        cleanup_and_exit();
    }

    fdA = gpiod_line_event_get_fd(encA);
    fdB = gpiod_line_event_get_fd(encB);
    if (fdA < 0 || fdB < 0) {
        fprintf(stderr, "[에러] 이벤트 파일 디스크립터 실패\n");
        cleanup_and_exit();
    }

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, encoder_thread, NULL);

    struct Point pos = {0, 0};
    int dir = NORTH;

    char cmd[32];
    printf("[명령 입력] GO / STOP / LEFT / RIGHT / EXIT\n");

    while (1) {
        printf(">>> ");
        if (!fgets(cmd, sizeof(cmd), stdin)) break;
        cmd[strcspn(cmd, "\n")] = 0;
        for (char *p = cmd; *p; ++p) *p = toupper(*p);

        if (strcmp(cmd, "GO") == 0) {
            reset_counts();
            forward_one(&pos, dir, SPEED);
            print_counts("GO");
        } else if (strcmp(cmd, "LEFT") == 0) {
            reset_counts();
            rotate_one(&dir, -1, SPEED);
            print_counts("LEFT");
        } else if (strcmp(cmd, "RIGHT") == 0) {
            reset_counts();
            rotate_one(&dir, 1, SPEED);
            print_counts("RIGHT");
        } else if (strcmp(cmd, "STOP") == 0) {
            motor_stop();
            print_counts("STOP");
        } else if (strcmp(cmd, "EXIT") == 0) {
            break;
        } else {
            printf("[오류] 알 수 없는 명령입니다.\n");
        }
    }

    running = 0;
    pthread_join(thread_id, NULL);
    cleanup_and_exit();
    return 0;
}