#ifndef SENSOR_H
#define SENSOR_H

#define SENSOR_COUNT 4

#include <stdbool.h>
#include <signal.h>

#ifdef __cplusplus
extern "C" {
#endif

// 기존 함수 선언
void handle_sigint(int signo);
void sensor_init(void);
void sensor_read_distances(float distances[SENSOR_COUNT]);
void sensor_activate_servo(int idx);
void sensor_cycle(void);
void sensor_cleanup(void);

// 센서 쓰레드가 계속 돌아갈지 제어하는 플래그
static volatile bool sensor_thread_running;

// 새로 추가: 임계값 기반 모니터링 루프
//  - dist_thresh: 거리 조건 (cm 이하)
//  - diff_thresh: 변화량 조건 (cm 이상)
//  - loop_delay_us: 한 사이클 후 대기 시간 (µs)
// 이 함수는 내부에서 sensor_read_distances() → 조건 검사 → printf() 를 반복합니다.
void sensor_monitor_triggers(float dist_thresh,
                             float diff_thresh,
                             unsigned int loop_delay_us,
                             volatile bool *run_flag);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_H