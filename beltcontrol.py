import RPi.GPIO as GPIO
import time

# 핀 설정
MOTOR_IN1 = 22   # L298N IN1
MOTOR_IN2 = 27   # L298N IN2
BUTTON = 17      # 버튼 입력

# GPIO 초기화
GPIO.setmode(GPIO.BCM)
GPIO.setup(MOTOR_IN1, GPIO.OUT)
GPIO.setup(MOTOR_IN2, GPIO.OUT)
GPIO.setup(BUTTON, GPIO.IN, pull_up_down=GPIO.PUD_UP)

try:
    print("버튼을 누르고 있으면 컨베이어 벨트가 회전합니다.")
    while True:
        if GPIO.input(BUTTON) == GPIO.LOW:
            print("버튼 눌림 → 모터 작동")
            GPIO.output(MOTOR_IN1, GPIO.HIGH)
            GPIO.output(MOTOR_IN2, GPIO.LOW)
        else:
            print("버튼 뗌 → 모터 정지")
            GPIO.output(MOTOR_IN1, GPIO.LOW)
            GPIO.output(MOTOR_IN2, GPIO.LOW)
        time.sleep(0.1)  # 디바운싱 및 CPU 낭비 방지

except KeyboardInterrupt:
    print("사용자에 의해 종료됨.")

finally:
    GPIO.cleanup()
    print("GPIO 클린업 완료")
