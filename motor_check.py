import RPi.GPIO as GPIO
import time

# 핀 번호 설정 (BCM 모드)
GPIO.setmode(GPIO.BCM)
GPIO.setwarnings(False)

# 모터 제어 핀 지정
IN1 = 17  # 왼쪽 모터 +
IN2 = 18  # 왼쪽 모터 -
IN3 = 22  # 오른쪽 모터 +
IN4 = 23  # 오른쪽 모터 -

# 핀 출력 모드 설정
motor_pins = [IN1, IN2, IN3, IN4]
for pin in motor_pins:
    GPIO.setup(pin, GPIO.OUT)

# 제어 함수들
def forward():
    GPIO.output(IN1, GPIO.HIGH)
    GPIO.output(IN2, GPIO.LOW)
    GPIO.output(IN3, GPIO.HIGH)
    GPIO.output(IN4, GPIO.LOW)

def backward():
    GPIO.output(IN1, GPIO.LOW)
    GPIO.output(IN2, GPIO.HIGH)
    GPIO.output(IN3, GPIO.LOW)
    GPIO.output(IN4, GPIO.HIGH)

def left():
    GPIO.output(IN1, GPIO.LOW)
    GPIO.output(IN2, GPIO.HIGH)
    GPIO.output(IN3, GPIO.HIGH)
    GPIO.output(IN4, GPIO.LOW)

def right():
    GPIO.output(IN1, GPIO.HIGH)
    GPIO.output(IN2, GPIO.LOW)
    GPIO.output(IN3, GPIO.LOW)
    GPIO.output(IN4, GPIO.HIGH)

def stop():
    for pin in motor_pins:
        GPIO.output(pin, GPIO.LOW)

# 테스트 실행
try:
    print("Forward")
    forward()
    time.sleep(2)

    print("Backward")
    backward()
    time.sleep(2)

    print("Left")
    left()
    time.sleep(2)

    print("Right")
    right()
    time.sleep(2)

    print("Stop")
    stop()
finally:
    GPIO.cleanup()
