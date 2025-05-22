import subprocess
import os
import sys
import time

from container_DB import connect_db, insert_distance
from container_Sensor import setup, measure_distance, cleanup
from container_MQTT_Handler import create_mqtt_client

if __name__ == "__main__":
    # 1) Start camera script
    script_dir = os.path.dirname(os.path.realpath(__file__))
    camera_script = os.path.join(script_dir, 'A_car_camer.py')
    camera_proc = subprocess.Popen([sys.executable, camera_script])
    print(f"▶️ Started A_car_camer.py (PID {camera_proc.pid})")

    # 2) DB connection
    conn, cursor = connect_db()

    # 3) Sensor setup
    setup()

    # 4) MQTT client start
    mqtt_client = create_mqtt_client((conn, cursor))
    mqtt_client.loop_start()

    try:
        while True:
            dist = measure_distance()
            print(f"🔍 Distance: {dist} cm")
            insert_distance(cursor, conn, dist)
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
            print("✔️ Camera script terminated.")
        print("✔️ Shutdown complete.")
