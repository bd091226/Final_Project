//libgpio로 모터 테스트
#include <gpiod.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

#define CHIPNAME "gpiochip0"

// BCM 핀 번호
#define PWMA 18
#define AIN1 22
#define AIN2 27
#define PWMB 23
#define BIN1 25
#define BIN2 24

struct gpiod_chip *chip;
struct gpiod_line *pwma, *ain1, *ain2, *pwmb, *bin1, *bin2;

// 디지털 출력 제어 함수
void set_line(struct gpiod_line *line, int value) {
    gpiod_line_set_value(line, value);
}

// "PWM" 흉내 내기 (단순 토글, 정확한 PWM 제어는 별도 구현 필요)
void pwm_write(struct gpiod_line *line, int duty_cycle) {
    int on_time = duty_cycle * 100;
    int off_time = (100 - duty_cycle) * 100;
    set_line(line, 1);
    usleep(on_time);
    set_line(line, 0);
    usleep(off_time);
}

void motor_go() {
    printf("go\n");
    set_line(ain1, 0);
    set_line(ain2, 1);
    set_line(bin1, 0);
    set_line(bin2, 1);

    // 지속 동작: PWM 핀 high
    set_line(pwma, 1);
    set_line(pwmb, 1);
}

void motor_back(int speed) {
    printf("back\n");
    set_line(ain1, 1);
    set_line(ain2, 0);
    set_line(bin1, 1);
    set_line(bin2, 0);
    pwm_write(pwma, speed);
    pwm_write(pwmb, speed);
}

void motor_left(int speed) {
    printf("left\n");
    set_line(ain1, 1);
    set_line(ain2, 0);
    set_line(bin1, 0);
    set_line(bin2, 1);
    pwm_write(pwma, speed);
    pwm_write(pwmb, speed);
}

void motor_right(int speed) {
    printf("right\n");
    set_line(ain1, 0);
    set_line(ain2, 1);
    set_line(bin1, 1);
    set_line(bin2, 0);
    pwm_write(pwma, speed);
    pwm_write(pwmb, speed);
}

void motor_stop() {
    printf("stop\n");
    // 방향 유지 상관없음. PWM만 끔.
    set_line(pwma, 0);
    set_line(pwmb, 0);
}

// GPIO 초기화
struct gpiod_line* init_line(const char* name, int offset) {
    struct gpiod_line* line = gpiod_chip_get_line(chip, offset);
    if (gpiod_line_request_output(line, name, 0) < 0) {
        perror("Line request failed");
        exit(1);
    }
    return line;
}

int main() {
    char cmd[20];
    int speed = 50;

    chip = gpiod_chip_open_by_name(CHIPNAME);
    if (!chip) {
        perror("Open chip failed");
        return 1;
    }

    // BCM GPIO 번호로 라인 초기화
    pwma = init_line("PWMA", PWMA);
    ain1 = init_line("AIN1", AIN1);
    ain2 = init_line("AIN2", AIN2);
    pwmb = init_line("PWMB", PWMB);
    bin1 = init_line("BIN1", BIN1);
    bin2 = init_line("BIN2", BIN2);

    while (1) {
        printf("Enter command (go/back/left/right/stop): ");
        fgets(cmd, sizeof(cmd), stdin);
        cmd[strcspn(cmd, "\n")] = 0;
    
        if (strcmp(cmd, "go") == 0) {
            motor_go(speed);  // 전진 → stop 명령 전까지 계속 유지됨
        } else if (strcmp(cmd, "back") == 0) {
            motor_back(speed);
        } else if (strcmp(cmd, "left") == 0) {
            motor_left(speed);
            sleep(3);     // 회전 후 자동 정지
            motor_stop();
        } else if (strcmp(cmd, "right") == 0) {
            motor_right(speed);
            sleep(3);     // 회전 후 자동 정지
            motor_stop();
        } else if (strcmp(cmd, "stop") == 0) {
            motor_stop();
        } else {
            printf("Unknown command.\n");
        }
    }    

    gpiod_chip_close(chip);
    return 0;
}
