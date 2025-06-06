#ifndef IMU_WRAPPER_H
#define IMU_WRAPPER_H

#ifdef __cplusplus
extern "C" {
#endif

// RTIMULib2를 이용해 IMU 초기화
void imu_init(void);

// IMU로부터 현재 yaw(라디안)를 반환. 오류 시 0.0f 반환
float imu_getYaw(void);

#ifdef __cplusplus
}
#endif

#endif // IMU_WRAPPER_H
