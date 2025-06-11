#include <gpiod.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#define CHIP_NAME "gpiochip0" // 기본 GPIO chip
#define GPIO_LINE 26          // BCM 번호 (ex: GPIO18)
#define PERIOD_MS 20          // 20ms 주기 (50Hz)

// 각도에 따라 HIGH 시간 (us) 계산
// int angle_to_pulse(int angle)
// {
//     if (angle < 0)
//         angle = 0;
//     if (angle > 180)
//         angle = 180;
//     return 500 + (angle * 2000 / 180); // 500~2500us
// }

int angle_to_pulse(int angle)
{
    // -90 ~ +90 범위를 0 ~ 180으로 변환
    if (angle < -90)
        angle = -90;
    if (angle > 90)
        angle = 90;
    
    int mapped_angle = angle + 90; // -90 -> 0, 0 -> 90, 90 -> 180
    
    return 500 + (mapped_angle * 2000 / 180); // 500~2500us
}


void move_servo(struct gpiod_line *line, int angle)
{
    int high_time = angle_to_pulse(angle);
    int low_time = PERIOD_MS * 1000 - high_time;

    // 10회 반복 (약 0.2초 유지)
    for (int i = 0; i < 10; ++i)
    {
        gpiod_line_set_value(line, 1); // HIGH
        usleep(high_time);
        gpiod_line_set_value(line, 0); // LOW
        usleep(low_time);
    }
}

int main()
{
    struct gpiod_chip *chip;
    struct gpiod_line *line;

    chip = gpiod_chip_open_by_name(CHIP_NAME);
    if (!chip)
    {
        perror("gpiod_chip_open_by_name");
        return 1;
    }

    line = gpiod_chip_get_line(chip, GPIO_LINE);
    if (!line)
    {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        return 1;
    }

    if (gpiod_line_request_output(line, "servo", 0) < 0)
    {
        perror("gpiod_line_request_output");
        gpiod_chip_close(chip);
        return 1;
    }

    printf("서보모터 0도\n");
    move_servo(line, 0);
    sleep(1);

    printf("서보모터 90도\n");
    move_servo(line,90);
    sleep(1);

    printf("서보모터 0도\n");
    move_servo(line, 0);
    sleep(1);

    gpiod_line_release(line);
    gpiod_chip_close(chip);

    return 0;
}
