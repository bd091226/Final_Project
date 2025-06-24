#include <gpiod.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/poll.h>
#include <sys/time.h>
#include <MQTTClient.h>
#include "moter_control.h"
#include "Bcar_moter.h"
#include "encoder.h"

const int DIR_VECTORS[4][2] = {
    {-1, 0}, // N
    {0, 1},  // E
    {1, 0},  // S
    {0, -1}  // W
};

static int gpio_initialized = 0;

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
// PWM 생성 함수 (서보모터 제어)
void generate_pwm(struct gpiod_line *line, int pulse_width_us, int duration_ms) {
    int cycles = duration_ms / 20;  // 20ms 기준(50Hz)
    for (int i = 0; i < cycles; i++) {
        gpiod_line_set_value(line, 1);
        usleep(pulse_width_us);
        gpiod_line_set_value(line, 0);
        usleep(20000 - pulse_width_us);
    }
}
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

void setup()
{
    if (gpio_initialized) return;

    // 1) 칩 열기
    chip = gpiod_chip_open_by_name(CHIP);
    if (!chip) { perror("gpiochip0 open failed"); exit(1); }

    // 2) 모터 제어용 라인 가져오기
    in1    = gpiod_chip_get_line(chip, IN1_PIN);
    in2    = gpiod_chip_get_line(chip, IN2_PIN);
    ena    = gpiod_chip_get_line(chip, ENA_PIN);
    in3    = gpiod_chip_get_line(chip, IN3_PIN);
    in4    = gpiod_chip_get_line(chip, IN4_PIN);
    enb    = gpiod_chip_get_line(chip, ENB_PIN);
    line_btn = gpiod_chip_get_line(chip, BUTTON_PIN);
    encA   = gpiod_chip_get_line(chip, ENCA);
    encB   = gpiod_chip_get_line(chip, ENCB);

    // 3) 모터 출력 요청
    if (gpiod_line_request_output(in1, "IN1", 0) < 0 ||
        gpiod_line_request_output(in2, "IN2", 0) < 0 ||
        gpiod_line_request_output(ena, "ENA", 0) < 0 ||
        gpiod_line_request_output(in3, "IN3", 0) < 0 ||
        gpiod_line_request_output(in4, "IN4", 0) < 0 ||
        gpiod_line_request_output(enb, "ENB", 0) < 0) {
        perror("모터 GPIO 요청 실패");
        exit(1);
    }

    // 4) 버튼 이벤트 요청
    if (gpiod_line_request_falling_edge_events(line_btn, "BUTTON") < 0) {
        perror("BUTTON 요청 실패");
        exit(1);
    }

    // 5) 엔코더 이벤트 요청 (both edges + pull-up)
    if (gpiod_line_request_both_edges_events_flags(
            encA, "ENCA", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP) < 0 ||
        gpiod_line_request_both_edges_events_flags(
            encB, "ENCB", GPIOD_LINE_REQUEST_FLAG_BIAS_PULL_UP) < 0) {
        perror("엔코더 이벤트 요청 실패");
        exit(1);
    }

    // 6) 엔코더 fd 얻고 poll() 배열 초기화
    fdA = gpiod_line_event_get_fd(encA);
    fdB = gpiod_line_event_get_fd(encB);
    pfds[0].fd     = fdA;
    pfds[0].events = POLLIN;
    pfds[1].fd     = fdB;
    pfds[1].events = POLLIN;

    gpio_initialized = 1;
}

void cleanup()
{
    if (!gpio_initialized) return;
    gpiod_line_release(in1);
    gpiod_line_release(in2);
    gpiod_line_release(ena);
    gpiod_line_release(in3);
    gpiod_line_release(in4);
    gpiod_line_release(enb);
    gpiod_line_release(line_btn);
    gpiod_line_release(servo_line);
    gpiod_line_release(encA);
    gpiod_line_release(encB);
    // gpiod_line_release(trig_line);
    // gpiod_line_release(echo_line);
    gpiod_chip_close(chip);
    in1 = in2 = ena = in3 = in4 = enb =
    line_btn = servo_line = encA = encB =
    trig_line = echo_line = NULL;
    chip = NULL;

    gpio_initialized = 0;
}

void set_speed(int speedA, int speedB)
{
    pwm_set_duty(ena, speedA);
    pwm_set_duty(enb, speedB);
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

    //int current_idx = current_dir;
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
        motor_stop();
        return current_dir;
    }

    int diff = (target_idx - current_dir + 4) % 4;
    int speed=40;

    // 현재 위치 구조체를 Point 형식으로 변환 (forward_one에서 필요)
    Point p;
    p.r = curr.y; // y → row
    p.c = curr.x; // x → col
    int dir = current_dir;

    switch (diff)
    {
    case 0:
        printf("⬆️ 직진\n");
        forward_one(&p, dir);  
        break;
    case 1:
        printf("➡️ 우회전\n");
        rotate_one(&dir, 1);
        //forward_one(&pos, dir);  
        break;
    case 2:
        printf("🔄 유턴\n");
        rotate_one(&dir, 1);
        rotate_one(&dir, 1);
        forward_one(&p, dir);  
        break;
    case 3:
        printf("⬅️ 좌회전\n");
        rotate_one(&dir, -1);
        forward_one(&p, dir);  
        break;
    default:
        printf("⚠️ 예외 상황\n");
        motor_stop();
        break;
    }

    usleep(700 * 1000);
    motor_stop();
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
        printf("경로[%d]: (%d, %d)\n", count, path[count].x, path[count].y);
        count++;
    }
    fclose(fp);
    return count;
}

int complete_message(const char *topic, const char *message)
{
    char payload[64];
    snprintf(payload, sizeof(payload), "%s", message);

    MQTTClient_message pubmsg = MQTTClient_message_initializer;
    pubmsg.payload = payload;
    pubmsg.payloadlen = (int)strlen(payload);
    pubmsg.qos = QOS;
    pubmsg.retained = 0;

    MQTTClient_deliveryToken token;
    int rc = MQTTClient_publishMessage(client, topic, &pubmsg, &token);
    if (rc != MQTTCLIENT_SUCCESS)
    {
        fprintf(stderr, "[오류] MQTT 메시지 발행 실패 (rc=%d)\n", rc);
        return 1;
    }

    printf("[송신] %s → %s\n", payload, topic);
}
int run_vehicle_path(const char *goal)
{
    if (!gpio_initialized) {
        setup();
        if (!gpio_initialized) {
        fprintf(stderr, "❌ setup() 실패: GPIO 초기화되지 않음\n");
        return 1;
    }
    }

    char path_filename[64];

    char goal_str[2] = {goal[0], '\0'}; // goal이 'K'이면 "K"로 바뀜
    snprintf(path_filename, sizeof(path_filename), "path_B_to_%s.txt", goal_str);

    printf("경로 파일: %s\n", path_filename);

    Position path[MAX_PATH_LENGTH];
    Direction current_dir = S; // 초기 방향 동쪽

    int path_len = load_path_from_file(path_filename, path);
    if (path_len <= 0)
    {
        fprintf(stderr, "경로 파일 읽기 실패\n");
        return 1;
    }
    printf("[차량 이동 시작: B → %s]\n", goal);
    for (int i = 0; i < path_len - 1; i++)
    {
        current_dir = move_step(path[i], path[i + 1], current_dir);
        // while (1)
        // {
        //     float distance = get_distance_cm();
        //     if (distance < 0)
        //     {
        //         fprintf(stderr, "거리 측정 실패\n");
        //         stop_motor();
        //         break;
        //     }
        //     printf("거리: %.2f cm\n", distance);

        //     if (distance < 10.0)
        //     {
        //         printf("거리 10cm 이하 - 차량 정지\n");
        //         stop_motor();
        //         delay_ms(100);
        //     }
        //     else
        //     {
        //         printf("이동 재개\n");
        //         current_dir = move_step(path[i], path[i + 1], current_dir);
        //         break; // 다음 위치로 진행
        //     }
        // }
    }

    servo_line = gpiod_chip_get_line(chip, SERVO_PIN);
    if (!servo_line) {
        perror("servo pin line request failed");
        gpiod_chip_close(chip);
        chip = NULL;
        return 1;
    }
    if (gpiod_line_request_output(servo_line, "servo", 0) < 0) {
        perror("servo line request output failed");
        gpiod_chip_close(chip);
        chip = NULL;
        return 1;
    }

    // printf("서보모터 0도\n");
    // move_servo(servo_line, 0);
    // usleep(1000000); // 1초 대기

    printf("서보모터 90도\n");
    move_servo(servo_line,750);
    usleep(1000000); // 1초 대기

    printf("서보모터 0도\n");
    move_servo(servo_line, 0);
    usleep(1000000); // 1초 대기



    snprintf(path_filename, sizeof(path_filename), "path_%s_to_B.txt", goal_str);
    printf("\n복귀 파일: %s\n", path_filename);

    path_len = load_path_from_file(path_filename, path);
    //current_dir = S; // 복귀 시 초기 방향을 남쪽 또는 적절히 설정

    if (path_len <= 0)
    {
        fprintf(stderr, "경로 파일 읽기 실패 (%s → B)\n", goal);
        return 1;
    }

    printf("[차량 복귀 시작: %s → B]\n", goal);

    for (int i = 0; i < path_len - 1; i++)
    {
        current_dir = move_step(path[i], path[i + 1], current_dir);
        // while (1)
        // {
        //     float distance = get_distance_cm();
        //     if (distance < 0)
        //     {
        //         fprintf(stderr, "거리 측정 실패\n");
        //         stop_motor();
        //         return 1;
        //     }

        //     printf("거리: %.2f cm\n", distance);

        //     if (distance < 10.0)
        //     {
        //         printf("거리 10cm 이하 - 차량 정지\n");
        //         stop_motor();
        //         delay_ms(100);
        //     }
        //     else
        //     {
        //         printf("이동 재개\n");
        //         current_dir = move_step(path[i], path[i + 1], current_dir);
        //         break;
        //     }
        // }
    }

    motor_stop();
    cleanup();
    complete_message(TOPIC_B_COMPLETED, "B차량 수행 완료");
    return 0;
}
