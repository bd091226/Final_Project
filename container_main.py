# container_main.py

import subprocess, os, sys, time
from container_DB import connect_db, insert_distance
from container_MQTT import create_mqtt_client
from container_Sensor import setup, measure_distance, cleanup
from container_config import TOPIC_PUB_DIST

if __name__ == "__main__":
    # 1) 카메라 실행
    script_dir    = os.path.dirname(os.path.realpath(__file__))
    camera_script = os.path.join(script_dir, 'container_camera.py')
    camera_proc   = subprocess.Popen([sys.executable, camera_script])

    # 2) DB & MQTT
    conn, cursor    = connect_db()
    mqtt_client     = create_mqtt_client((conn, cursor))
    mqtt_client.loop_start()

    # 3) 센서 초기화
    setup()

    try:
        while True:
            dist = measure_distance()
            print(f"🔍 Distance: {dist} cm")
            insert_distance(cursor, conn, dist)

            # ─── 거리 5cm 미만일 때 C차 출발 퍼블리시 ─────────────────
            if dist < 5:
                mqtt_client.publish(TOPIC_PUB_DIST, "C차 출발", qos=1)
                print(f"⚡ Published 'C차 출발' to {TOPIC_PUB_DIST}")
            # ──────────────────────────────────────────────────

            time.sleep(1)

    except KeyboardInterrupt:
        pass

    finally:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        cleanup()
        cursor.close()
        conn.close()
        if camera_proc.poll() is None:
            camera_proc.terminate()
        print("✔️ Shutdown complete.")
