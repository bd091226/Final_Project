#include "imu_wrapper.h"
#include "RTIMULib.h"
#include <stdlib.h>

// RTIMULib2 전용 객체 (전역으로 보관)
static RTIMUSettings *settings = NULL;
static RTIMU       *imu      = NULL;
static RTIMU_DATA   imuData;  // IMU 데이터를 저장할 구조체

extern "C" {

// 1) IMU 초기화: RTIMULib2 설정 → IMU 생성 → IMUInit → 필터 설정 등
void imu_init(void) {
    // settings 폴더/파일 이름: "RTIMULib" (RTIMULib.ini 등을 자동 로드)
    settings = new RTIMUSettings("RTIMULib");
    imu = RTIMU::createIMU(settings);
    if (!imu) {
        // 생성 실패 시 그대로 리턴
        return;
    }
    if (!imu->IMUInit()) {
        // 초기화 실패 시 그대로 리턴
        return;
    }
    // 보정 필터 파라미터
    imu->setSlerpPower(0.02f);
    imu->setGyroEnable(true);
    imu->setAccelEnable(true);
    imu->setCompassEnable(false);
}

// 2) IMU에서 yaw(라디안) 얻기
float imu_getYaw(void) {
    if (imu && imu->IMURead()) {
        // IMURead()가 true를 반환하면 내부 버퍼 imuData에 새 데이터가 채워짐
        imuData = imu->getIMUData();
        // fusionPose는 RTVector3 타입. yaw는 z() 메서드를 이용해 가져옴
        return imuData.fusionPose.z();
    }
    return 0.0f;
}

} // extern "C"
