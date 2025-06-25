#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/poll.h>
#include <time.h>

#define CHIP       "gpiochip0"
#define IN1_PIN    17
#define IN2_PIN    18
#define ENA_PIN    12
#define IN3_PIN    22
#define IN4_PIN    23
#define ENB_PIN    13
#define ENC_A_PIN  14
#define ENC_B_PIN  15

#define PWM_PERIOD_US 2000  // 500 Hz
#define SEC_TO_US(s)   ((useconds_t)((s) * 1e6))

volatile int countA = 0, countB = 0;
struct gpiod_chip *chip;
struct gpiod_line *in1, *in2, *ena, *in3, *in4, *enb;
struct gpiod_line *encA, *encB;

long get_microseconds() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC_RAW, &ts);
    return ts.tv_sec * 1000000L + ts.tv_nsec / 1000L;
}

void reset_counts() {
    countA = countB = 0;
}

void safe_set(struct gpiod_line *line, int val) {
    if (gpiod_line_set_value(line, val) < 0) {
        perror("GPIO set");
        exit(1);
    }
}

void motor_stop() {
    safe_set(in1, 0); safe_set(in2, 0);
    safe_set(in3, 0); safe_set(in4, 0);
    safe_set(ena,  0); safe_set(enb,  0);
}

void dir_forward() {
    safe_set(in1, 0); safe_set(in2, 1);
    safe_set(in3, 0); safe_set(in4, 1);
}

void test_speed(int speed_pct, double duration_s) {
    reset_counts();
    dir_forward();
    struct pollfd pfds[2] = {
        { .fd = gpiod_line_event_get_fd(encA), .events = POLLIN },
        { .fd = gpiod_line_event_get_fd(encB), .events = POLLIN }
    };
    struct gpiod_line_event evt;
    long end = get_microseconds() + SEC_TO_US(duration_s);
    while (get_microseconds() < end) {
        int high_us = PWM_PERIOD_US * speed_pct / 100;
        int low_us  = PWM_PERIOD_US - high_us;
        // ON phase
        safe_set(ena, 1); safe_set(enb, 1);
        long t0 = get_microseconds();
        while (get_microseconds() - t0 < high_us) {
            if (poll(pfds, 2, 0) > 0) {
                if (pfds[0].revents & POLLIN) { gpiod_line_event_read(encA, &evt); countA++; }
                if (pfds[1].revents & POLLIN) { gpiod_line_event_read(encB, &evt); countB++; }
            }
        }
        // OFF phase
        safe_set(ena, 0); safe_set(enb, 0);
        t0 = get_microseconds();
        while (get_microseconds() - t0 < low_us) {
            if (poll(pfds, 2, 0) > 0) {
                if (pfds[0].revents & POLLIN) { gpiod_line_event_read(encA, &evt); countA++; }
                if (pfds[1].revents & POLLIN) { gpiod_line_event_read(encB, &evt); countB++; }
            }
        }
    }
    motor_stop();
    printf("speed=%d%% -> A=%d (%.1f pps), B=%d (%.1f pps)\n",
           speed_pct, countA, countA/duration_s, countB, countB/duration_s);
}

int main() {
    signal(SIGINT, SIG_IGN);
    chip = gpiod_chip_open_by_name(CHIP);
    if (!chip) { perror("chip open"); return 1; }
    in1  = gpiod_chip_get_line(chip, IN1_PIN);
    in2  = gpiod_chip_get_line(chip, IN2_PIN);
    ena  = gpiod_chip_get_line(chip, ENA_PIN);
    in3  = gpiod_chip_get_line(chip, IN3_PIN);
    in4  = gpiod_chip_get_line(chip, IN4_PIN);
    enb  = gpiod_chip_get_line(chip, ENB_PIN);
    encA = gpiod_chip_get_line(chip, ENC_A_PIN);
    encB = gpiod_chip_get_line(chip, ENC_B_PIN);
    if (!in1||!in2||!ena||!in3||!in4||!enb||!encA||!encB) {
        fprintf(stderr, "라인 요청 실패\n");
        return 1;
    }
    gpiod_line_request_output(in1, "test", 0);
    gpiod_line_request_output(in2, "test", 0);
    gpiod_line_request_output(ena, "test", 0);
    gpiod_line_request_output(in3, "test", 0);
    gpiod_line_request_output(in4, "test", 0);
    gpiod_line_request_output(enb, "test", 0);
    gpiod_line_request_both_edges_events_flags(encA, "encA", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);
    gpiod_line_request_both_edges_events_flags(encB, "encB", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP);

    // 실제 테스트
    test_speed(50, 1.0);
    motor_stop();
    test_speed(50, 1.0);
    motor_stop();
    test_speed(50, 1.0);
    motor_stop();
    test_speed(70, 1.0);
    motor_stop();
    test_speed(70, 1.0);
    motor_stop();
    test_speed(70, 1.0);
    motor_stop();
    gpiod_chip_close(chip);
    return 0;
}
