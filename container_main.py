# container_main.py

import subprocess
import os
import sys
import time

from container_DB import connect_db, insert_distance
from container_MQTT import create_mqtt_client
from container_Sensor import setup, measure_distance, cleanup

if __name__ == "__main__":
    # 1) Start camera script
    script_dir    = os.path.dirname(os.path.realpath(__file__))
    camera_script = os.path.join(script_dir, 'container_camera.py')
    camera_proc   = subprocess.Popen([sys.executable, camera_script])
    print(f"▶️ Started container_camera.py (PID {camera_proc.pid})")

    # 2) DB 연결 & MQTT 클라이언트 시작
    conn, cursor    = connect_db()
    mqtt_client     = create_mqtt_client((conn, cursor))
    mqtt_client.loop_start()

    # 3) Sensor 초기화
    setup()

    try:
        while True:
            # 4) 거리 측정 → DB 저장
            dist = measure_distance()
            print(f"🔍 Distance: {dist} cm")
            insert_distance(cursor, conn, dist)
            time.sleep(1)

    except KeyboardInterrupt:
        pass

    finally:
        # 5) 정리: MQTT, GPIO, DB, 카메라 프로세스
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        cleanup()
        cursor.close()
        conn.close()
        if camera_proc.poll() is None:
            camera_proc.terminate()
        print("✔️ Shutdown complete.")
