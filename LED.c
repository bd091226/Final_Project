#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> // for sleep()

#define GPIO_CHIP   "/dev/gpiochip0"
#define GPIO_LINE1  26 // 빨강
#define GPIO_LINE2  6 // 하양
#define GPIO_LINE3  16 // 초록

int main(void) {
    struct gpiod_chip  *chip;
    struct gpiod_line  *line1, *line2, *line3;
    int ret;

    // 1) GPIO 칩 열기
    chip = gpiod_chip_open(GPIO_CHIP);
    if (!chip) {
        perror("gpiod_chip_open");
        exit(EXIT_FAILURE);
    }

    // 2) 각 GPIO 라인 가져오기
    line1 = gpiod_chip_get_line(chip, GPIO_LINE1);
    line2 = gpiod_chip_get_line(chip, GPIO_LINE2);
    line3 = gpiod_chip_get_line(chip, GPIO_LINE3);
    if (!line1 || !line2 || !line3) {
        perror("gpiod_chip_get_line");
        gpiod_chip_close(chip);
        exit(EXIT_FAILURE);
    }

    // 3) 각 라인을 출력(output) 모드로 요청 (초기 값은 0 = OFF)
    ret = gpiod_line_request_output(line1, "led_blink", 0);
    ret |= gpiod_line_request_output(line2, "led_blink", 0);
    ret |= gpiod_line_request_output(line3, "led_blink", 0);
    if (ret < 0) {
        perror("gpiod_line_request_output");
        gpiod_chip_close(chip);
        exit(EXIT_FAILURE);
    }

    // 4) 무한 루프: 2초마다 OFF ↔ ON 반복
    while (1) {
        // LED 끄기
        gpiod_line_set_value(line1, 0);
        gpiod_line_set_value(line2, 0);
        gpiod_line_set_value(line3, 0);
        sleep(2);

        // LED 켜기
        gpiod_line_set_value(line1, 1);
        gpiod_line_set_value(line2, 1);
        gpiod_line_set_value(line3, 1);
        sleep(2);
    }

    // (도달하지 않는 코드) 리소스 정리
    gpiod_line_release(line1);
    gpiod_line_release(line2);
    gpiod_line_release(line3);
    gpiod_chip_close(chip);
    return 0;
}
