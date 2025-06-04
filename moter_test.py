import RPi.GPIO as GPIO
import time

# 핀 설정 (예시)
ENA = 12  # 왼쪽 모터 PWM 제어 핀 (ENA)
IN1 = 17  # 왼쪽 모터 방향 제어
IN2 = 18

ENB = 13  # 오른쪽 모터 PWM 제어 핀 (ENB)
IN3 = 22  # 오른쪽 모터 방향 제어
IN4 = 23

GPIO.setmode(GPIO.BCM)

# 방향 제어 핀 출력 설정
GPIO.setup([IN1, IN2, IN3, IN4], GPIO.OUT)

# PWM 핀 출력 설정
GPIO.setup(ENA, GPIO.OUT)
GPIO.setup(ENB, GPIO.OUT)

# PWM 객체 생성 (100Hz)
pwm_left = GPIO.PWM(ENA, 100)
pwm_right = GPIO.PWM(ENB, 100)
pwm_left.start(0)
pwm_right.start(0)

def stop():
    pwm_left.ChangeDutyCycle(0)
    pwm_right.ChangeDutyCycle(0)
    GPIO.output([IN1, IN2, IN3, IN4], GPIO.LOW)

def forward(speed):
    GPIO.output(IN1, GPIO.HIGH)
    GPIO.output(IN2, GPIO.LOW)
    GPIO.output(IN3, GPIO.HIGH)
    GPIO.output(IN4, GPIO.LOW)

    pwm_left.ChangeDutyCycle(speed)
    pwm_right.ChangeDutyCycle(speed)

def backward(speed):
    GPIO.output(IN1, GPIO.LOW)
    GPIO.output(IN2, GPIO.HIGH)
    GPIO.output(IN3, GPIO.LOW)
    GPIO.output(IN4, GPIO.HIGH)

    pwm_left.ChangeDutyCycle(speed)
    pwm_right.ChangeDutyCycle(speed)

def left(speed):
    GPIO.output(IN1, GPIO.LOW)
    GPIO.output(IN2, GPIO.HIGH)
    GPIO.output(IN3, GPIO.HIGH)
    GPIO.output(IN4, GPIO.LOW)

    pwm_left.ChangeDutyCycle(speed)
    pwm_right.ChangeDutyCycle(speed)

def right(speed):
    GPIO.output(IN1, GPIO.HIGH)
    GPIO.output(IN2, GPIO.LOW)
    GPIO.output(IN3, GPIO.LOW)
    GPIO.output(IN4, GPIO.HIGH)

    pwm_left.ChangeDutyCycle(speed)
    pwm_right.ChangeDutyCycle(speed)

# 테스트 코드
try:
    forward(40)
    time.sleep(2)
    stop()
    time.sleep(1)
    backward(40)
    time.sleep(2)
    stop()
finally:
    stop()
    pwm_left.stop()
    pwm_right.stop()
    GPIO.cleanup()
