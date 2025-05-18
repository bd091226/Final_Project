import RPi.GPIO as GPIO
import time

# GPIO 핀 번호 설정 (BCM 모드 기준)
IN3 = 17   # L298N IN1
IN4 = 27   # L298N IN2
BUTTON = 22  # 버튼 입력

# GPIO 초기화
GPIO.setmode(GPIO.BCM)
GPIO.setup(IN3, GPIO.OUT)
GPIO.setup(IN4, GPIO.OUT)
GPIO.setup(BUTTON, GPIO.IN, pull_up_down=GPIO.PUD_DOWN)

try:
    print("버튼을 누르고 있으면 컨베이어 벨트가 회전합니다.")
    while True:
        if GPIO.input(BUTTON) == GPIO.HIGH:
            # 버튼 눌림 → 정방향 회전
            GPIO.output(IN3, GPIO.HIGH)
            GPIO.output(IN4, GPIO.LOW)
        else:
            # 버튼 안 눌림 → 정지
            GPIO.output(IN3, GPIO.LOW)
            GPIO.output(IN4, GPIO.LOW)
        time.sleep(0.05)

finally:
    # 종료 시 GPIO 초기화
    GPIO.cleanup()
    print("GPIO 클린업 완료")
