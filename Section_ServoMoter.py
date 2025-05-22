import RPi.GPIO as GPIO
import time

# 서보 연결 핀 (BCM)
SERVO_PIN = 18

# 각도 → 듀티사이클 변환 함수
def angle_to_duty(angle):
    # 0° → 2.5% (≈0.5ms), 180° → 12.5% (≈2.5ms)
    return 2.5 + (angle / 180.0) * 10.0

GPIO.setmode(GPIO.BCM)
GPIO.setup(SERVO_PIN, GPIO.OUT)

# 50Hz PWM 시작
p = GPIO.PWM(SERVO_PIN, 50)
p.start(angle_to_duty(0))  # 처음 위치: 0°

try:
    angles = [0, 90, 180, 90, 0]
    while True:
        for angle in angles:
            dc = angle_to_duty(angle)
            print(f"▶️ Moving to {angle}° (DC={dc:.1f}%)")
            p.ChangeDutyCycle(dc)
            time.sleep(1)        # 목표 각도에 도달할 시간
        # 루프가 끝나도 PWM 은 꺼지지 않고 유지됩니다.

except KeyboardInterrupt:
    print("\n⏹️ KeyboardInterrupt: Stopping servo sweep")

finally:
    p.stop()
    GPIO.cleanup()
    print("✔️ Cleanup done. Servo PWM stopped.")
