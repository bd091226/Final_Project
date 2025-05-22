import subprocess, os, sys, time
from container_Sensor import run_sensor_loop
from container_DB import connect_db
from container_MQTT import create_mqtt_client

if __name__ == "__main__":
    # 1) 카메라 실행
    script_dir = os.path.dirname(os.path.realpath(__file__))
    camera_script = os.path.join(script_dir, 'container_camera.py')
    camera_proc = subprocess.Popen([sys.executable, camera_script])

    # 2) DB & MQTT 연결
    conn, cursor = connect_db()
    mqtt_client = create_mqtt_client((conn, cursor))
    mqtt_client.loop_start()

    try:
        # 3) 센서 루프 실행
        run_sensor_loop(mqtt_client, conn, cursor)
    except KeyboardInterrupt:
        pass
    finally:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        cursor.close()
        conn.close()
        if camera_proc.poll() is None:
            camera_proc.terminate()
        print("🛑 시스템 종료 완료.")
