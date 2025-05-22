import subprocess, os, sys
from container_DB import connect_db
from container_MQTT_Sensor import initialize_gpio, create_mqtt_client_with_servo, run_sensor_loop

if __name__ == "__main__":
    # ✅ 0) GPIO 초기화
    initialize_gpio()  # ← 이걸 꼭 추가하세요

    # 1) 카메라 서브프로세스 실행
    script_dir = os.path.dirname(os.path.realpath(__file__))
    camera_script = os.path.join(script_dir, 'container_camera.py')
    camera_proc = subprocess.Popen([sys.executable, camera_script])

    # 2) DB & MQTT & 서보모터 초기화
    conn, cursor = connect_db()
    mqtt_client, pwm = create_mqtt_client_with_servo((conn, cursor))
    mqtt_client.loop_start()

    try:
        run_sensor_loop(mqtt_client, conn, cursor)
    except KeyboardInterrupt:
        print("🛑 프로그램 종료 중...")
    finally:
        mqtt_client.loop_stop()
        mqtt_client.disconnect()
        pwm.stop()
        cursor.close()
        conn.close()
        if camera_proc.poll() is None:
            camera_proc.terminate()
        print("✅ 시스템 종료 완료.")
        from RPi import GPIO
        GPIO.cleanup()
