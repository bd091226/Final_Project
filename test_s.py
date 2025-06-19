# 데베랑 거리센서

import RPi.GPIO as GPIO
import time
import mysql.connector
from mysql.connector import Error

# GPIO 핀 번호 설정 (BCM 모드)
GPIO.setmode(GPIO.BCM)
TRIG_PIN = 23  # Trigger 핀 (BCM 23)
ECHO_PIN = 24  # Echo 핀    (BCM 24)
GPIO.setup(TRIG_PIN, GPIO.OUT)
GPIO.setup(ECHO_PIN, GPIO.IN)

# DB 연결 정보 수정: database를 실제 스키마(Project_19)로 변경
DB_CONFIG = {
    "user": "Project_19",      # MySQL 사용자 계정명
    "password": "1234",        # 실제 비밀번호
    "host": "192.168.137.148",
    "database": "Project_19",  # 올바른 데이터베이스(스키마) 이름
    "charset": "utf8"
}

def connect_db():
    """MySQL에 연결하고 cursor 반환. 테이블이 없으면 생성 후 반환."""
    while True:
        try:
            conn = mysql.connector.connect(**DB_CONFIG)
            if conn.is_connected():
                print(f"▶️ MySQL 서버에 연결되었습니다 (DB: {DB_CONFIG['database']}).")
                cursor = conn.cursor()
                # 테이블이 없으면 생성
                cursor.execute(
                    """
                    CREATE TABLE IF NOT EXISTS distance_measurements (
                      id INT AUTO_INCREMENT PRIMARY KEY,
                      distance FLOAT NOT NULL,
                      measured_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
                    )
                    """
                )
                conn.commit()
                return conn, cursor
        except Error as e:
            print(f"❌ MySQL 연결 실패: {e}. 5초 후 재시도합니다...")
            time.sleep(5)


def measure_distance():
    """초음파 센서로부터 거리(cm)를 측정"""
    GPIO.output(TRIG_PIN, False)
    time.sleep(0.5)
    GPIO.output(TRIG_PIN, True)
    time.sleep(0.00001)
    GPIO.output(TRIG_PIN, False)
    while GPIO.input(ECHO_PIN) == 0:
        pulse_start = time.time()
    while GPIO.input(ECHO_PIN) == 1:
        pulse_end = time.time()
    return round((pulse_end - pulse_start) * 17150, 2)


def main():
    conn, cursor = connect_db()
    try:
        while True:
            dist = measure_distance()
            print(f"측정된 거리: {dist} cm")
            if not conn.is_connected():
                try:
                    conn.reconnect(attempts=3, delay=5)
                    print("🔄 MySQL 재접속 성공")
                except Error as e:
                    print(f"❌ 재접속 실패: {e}. 새로운 연결 시도...")
                    conn.close()
                    conn, cursor = connect_db()
            try:
                cursor.execute(
                    "INSERT INTO distance_measurements (distance) VALUES (%s)",
                    (dist,)
                )
                conn.commit()
            except Error as e:
                print(f"❌ 데이터베이스 작업 중 오류 발생: {e}")
                conn.close()
                conn, cursor = connect_db()
            time.sleep(1)
    except KeyboardInterrupt:
        print("\n🚪 프로그램 종료 중...")
    finally:
        GPIO.cleanup()
        try:
            cursor.close()
            conn.close()
        except:
            pass
        print("✔️ 리소스 정리 완료.")

if __name__ == "__main__":
    main()
