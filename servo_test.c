#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

#define CHIP_NAME "gpiochip0"
#define GPIO_LINE 4   // BCM GPIO 4번 사용
#define PERIOD_MS 20  // 20ms = 50Hz
#define DUTY_CW 1.65  // 시계 방향 회전 (1.65ms)
#define DUTY_CCW 1.35 // 반시계 방향 회전 (1.35ms)
#define DUTY_STOP 1.5 // 정지 (1.5ms)

void delay_us(long us)
{
    struct timespec ts;
    ts.tv_sec = us / 1000000;
    ts.tv_nsec = (us % 1000000) * 1000;
    nanosleep(&ts, NULL);
}

void send_pwm(struct gpiod_line *line, float duty_ms, int repeat_count)
{
    for (int i = 0; i < repeat_count; i++)
    {
        gpiod_line_set_value(line, 1);
        delay_us((long)(duty_ms * 1000));
        gpiod_line_set_value(line, 0);
        delay_us((long)((PERIOD_MS - duty_ms) * 1000));
    }
}

int main()
{
    struct gpiod_chip *chip;
    struct gpiod_line *line;

    chip = gpiod_chip_open_by_name(CHIP_NAME);
    if (!chip)
    {
        perror("chip open error");
        return 1;
    }

    line = gpiod_chip_get_line(chip, GPIO_LINE);
    if (!line)
    {
        perror("line open error");
        gpiod_chip_close(chip);
        return 1;
    }

    if (gpiod_line_request_output(line, "servo", 0) < 0)
    {
        perror("line request error");
        gpiod_chip_close(chip);
        return 1;
    }

    printf("➡️ 시계 방향 회전 (0도 → 90도)\n");
    send_pwm(line, DUTY_CW, 10); // 약 90도 이동 (실험적으로 맞춘 값)

    printf("⏹ 정지\n");
    send_pwm(line, DUTY_STOP, 5); // 정지

    sleep(1); // 잠시 대기

    printf("⬅️ 반시계 방향 회전 (90도 → 0도)\n");
    send_pwm(line, DUTY_CCW, 80); // 반대방향으로 회전 (복귀)

    printf("⏹ 정지\n");
    send_pwm(line, DUTY_STOP, 5); // 정지

    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return 0;
}
