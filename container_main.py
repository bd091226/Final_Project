import subprocess, os, sys, time
from container_DB import connect_db
from container_MQTT_Sensor import create_mqtt_client, run_sensor_loop

if __name__ == "__main__":
    # 1) 카메라 서브프로세스 실행
    script_dir = os.path.dirname(os.path.realpath(__file__))
    camera_script = os.path.join(script_dir, 'container_camera.py')
    camera_proc = subprocess.Popen([sys.executable, camera_script])

    # 2) DB & MQTT 연결
    conn, cursor = connect_db()
    mqtt_client = create_mqtt_client((conn, cursor))
    mqtt_client.loop_start()

    try:
        # 3) 초음파 센서 루프 시작
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
