#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>

#define GPIO_CHIP_NAME  "gpiochip0"  // Raspberry Pi 기본 GPIO 칩
#define SERVO_OFFSET     18           // BCM GPIO4 → libgpiod offset 4
#define PWM_FREQ_HZ     50           // 서보모터 표준 주파수 50Hz
#define PWM_PERIOD_US  (1000000 / PWM_FREQ_HZ)  // 20,000 μs

/**
 * @brief  각도(0~180) 에 해당하는 펄스 폭(us)를 계산
 *         - 보통 0도: 1.0ms (1000us), 180도: 2.0ms (2000us) 가 표준 범위
 *         - 여기서는 1.0ms~2.0ms 사이 비례할당
 */
static int angle_to_pulse_width(int angle_deg) {
    if (angle_deg < 0)   angle_deg = 0;
    if (angle_deg > 180) angle_deg = 180;
    // 1000us + (angle / 180 * 1000us)
    return 1000 + (angle_deg * 1000) / 180;
}

/**
 * @brief  한 번의 PWM 펄스(pulse_width_us)를 GPIO로 출력
 * @param  line           : 요청된 GPIO line (출력 요청 상태)
 * @param  pulse_width_us : high 상태 유지 시간 (마이크로초 단위)
 */
static void send_single_pulse(struct gpiod_line *line, int pulse_width_us) {
    // HIGH
    gpiod_line_set_value(line, 1);
    usleep(pulse_width_us);

    // LOW (나머지 주기 동안)
    gpiod_line_set_value(line, 0);
    usleep(PWM_PERIOD_US - pulse_width_us);
}

int main(void) {
    struct gpiod_chip  *chip;
    struct gpiod_line  *line;
    int                 ret;

    // 1) GPIO 칩 열기
    chip = gpiod_chip_open_by_name(GPIO_CHIP_NAME);
    if (!chip) {
        perror("GPIO 칩 오픈 실패");
        return EXIT_FAILURE;
    }

    // 2) GPIO4 라인 요청 (output, 초기 low)
    line = gpiod_chip_get_line(chip, SERVO_OFFSET);
    if (!line) {
        perror("GPIO 라인 가져오기 실패");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    ret = gpiod_line_request_output(line, "servo-test", 0);
    if (ret < 0) {
        perror("GPIO 라인 출력 요청 실패");
        gpiod_chip_close(chip);
        return EXIT_FAILURE;
    }

    printf("서보 테스트 시작 (GPIO4 → 0°, 90°, 180° 순차 출력)\n");

    while (1) {
        // 0도 위치 (1.0ms 펄스) : 0° 대응
        printf("→ 0도 위치\n");
        for (int i = 0; i < PWM_FREQ_HZ; i++) {
            int pw = angle_to_pulse_width(0);
            send_single_pulse(line, pw);
        }
        sleep(1);

        // 90도 위치 (1.5ms 펄스)
        printf("→ 90도 위치\n");
        for (int i = 0; i < PWM_FREQ_HZ; i++) {
            int pw = angle_to_pulse_width(90);
            send_single_pulse(line, pw);
        }
        sleep(1);

        // 180도 위치 (2.0ms 펄스)
        printf("→ 180도 위치\n");
        for (int i = 0; i < PWM_FREQ_HZ; i++) {
            int pw = angle_to_pulse_width(180);
            send_single_pulse(line, pw);
        }
        sleep(1);
    }

    // (실제로는 여기까지 도달하지 않음)
    gpiod_line_release(line);
    gpiod_chip_close(chip);
    return EXIT_SUCCESS;
}
