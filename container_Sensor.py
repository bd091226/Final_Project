import RPi.GPIO as GPIO
import time
from container_config import TOPIC_PUB_DIST
from container_DB import insert_distance

TRIG_PIN = 23
ECHO_PIN = 24

def setup():
    """
    Initialize GPIO pins for ultrasonic sensor.
    """
    GPIO.setmode(GPIO.BCM)
    GPIO.setup(TRIG_PIN, GPIO.OUT)
    GPIO.setup(ECHO_PIN, GPIO.IN)

def measure_distance():
    """
    Measure distance in cm via ultrasonic sensor.
    """
    GPIO.output(TRIG_PIN, False)
    time.sleep(0.5)
    GPIO.output(TRIG_PIN, True)
    time.sleep(0.00001)
    GPIO.output(TRIG_PIN, False)

    while GPIO.input(ECHO_PIN) == 0:
        pulse_start = time.time()
    while GPIO.input(ECHO_PIN) == 1:
        pulse_end = time.time()

    distance = round((pulse_end - pulse_start) * 17150, 2)
    return distance

def cleanup():
    """
    Clean up GPIO resources.
    """
    GPIO.cleanup()
    
def run_sensor_loop(mqtt_client, conn, cursor):
    setup()
    try:
        while True:
            dist = measure_distance()
            print(f"📏 거리 측정: {dist} cm")

            insert_distance(cursor, conn, dist)

            if dist < 5:
                mqtt_client.publish(TOPIC_PUB_DIST, "B차 출발", qos=1)
                print(f"🚗 MQTT 발행: 'B차 출발' → {TOPIC_PUB_DIST}")

            time.sleep(1)
    finally:
        cleanup()
    