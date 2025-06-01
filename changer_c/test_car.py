## 전진 후진 좌회전(180도) 우회전(180도) 테스트하는 파일!!

from gpiozero import DigitalOutputDevice, PWMOutputDevice
import time

# 모터 핀 설정
PWMA = PWMOutputDevice(18)
AIN1 = DigitalOutputDevice(22)
AIN2 = DigitalOutputDevice(27)

PWMB = PWMOutputDevice(23)
BIN1 = DigitalOutputDevice(25)
BIN2 = DigitalOutputDevice(24)

def motor_go(speed=0.5):
    print("go")
    AIN1.value = 0
    AIN2.value = 1
    PWMA.value = speed
    BIN1.value = 0
    BIN2.value = 1
    PWMB.value = speed

def motor_back(speed=0.5):
    print("back")
    AIN1.value = 1
    AIN2.value = 0
    PWMA.value = speed
    BIN1.value = 1
    BIN2.value = 0
    PWMB.value = speed

def motor_left(speed=0.5):
    print("left")
    AIN1.value = 1
    AIN2.value = 0
    PWMA.value = speed
    BIN1.value = 0
    BIN2.value = 1
    PWMB.value = speed

def motor_right(speed=0.5):
    print("right")
    AIN1.value = 0
    AIN2.value = 1
    PWMA.value = speed
    BIN1.value = 1
    BIN2.value = 0
    PWMB.value = speed

def motor_stop():
    print("stop")
    AIN1.value = 0
    AIN2.value = 1
    PWMA.value = 0.0
    BIN1.value = 0
    BIN2.value = 1
    PWMB.value = 0.0

try:
    while True:
        cmd = input("Enter command (go/back/left/right/stop): ").strip().lower()
        
        if cmd == "go":
            motor_go()
        elif cmd == "back":
            motor_back()
        elif cmd == "left":
            motor_left()
            time.sleep(3.4)     # ← 2초 동안 유지
            motor_stop()  
        elif cmd == "right":
            motor_right()
            time.sleep(3.4)     # ← 2초 동안 유지
            motor_stop()  
        elif cmd == "stop":
            motor_stop()
        else:
            print("Unknown command.")

except KeyboardInterrupt:
    pass

# 안전하게 모터 정지
motor_stop()
