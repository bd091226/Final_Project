import RPi.GPIO as GPIO
import time

# 서보모터 제어에 사용할 GPIO 핀 번호
servo_pin = 12

# GPIO 설정
GPIO.setmode(GPIO.BCM)
GPIO.setup(servo_pin, GPIO.OUT)

# PWM 주파수는 50Hz로 설정
pwm = GPIO.PWM(servo_pin, 50)
pwm.start(0)

def set_angle(angle):
    # 각도를 듀티사이클로 변환 (SG90 기준)
    duty = 2 + (angle / 18)
    pwm.ChangeDutyCycle(duty)
    time.sleep(0.5)
    pwm.ChangeDutyCycle(0)

try:
    print("서보모터를 0도에서 180도로 회전시킵니다.")
    set_angle(0)
    time.sleep(1)

    set_angle(90)
    time.sleep(1)

    set_angle(180)
    time.sleep(1)

    print("다시 0도로 회전합니다.")
    set_angle(0)
    time.sleep(1)

except KeyboardInterrupt:
    print("사용자에 의해 종료됨.")

finally:
    pwm.stop()
    GPIO.cleanup()
