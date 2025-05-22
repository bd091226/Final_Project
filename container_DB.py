# container_DB.py

import mysql.connector
from mysql.connector import Error
import time
from container_config import DB_CONFIG

def connect_db():
    """
    Connect to MySQL, retrying every 5 seconds on failure.
    Returns (conn, cursor).
    """
    while True:
        try:
            conn = mysql.connector.connect(**DB_CONFIG)
            if conn.is_connected():
                return conn, conn.cursor()
        except Error:
            time.sleep(5)

def update_load_count(cursor, conn, count):
    cursor.execute(
        "UPDATE vehicle_status_A SET load_count = %s WHERE vehicle_id = 1",
        (count,)
    )
    conn.commit()

def insert_distance(cursor, conn, dist):
    cursor.execute(
        "INSERT INTO z_Seoul (distance, measured_at) VALUES (%s, NOW())",
        (dist,)
    )
    conn.commit()

def handle_qr_insert(type_, data):
    """
    QR 수신 시 호출.
    type_ 값에 따라 destination_zone_id 매핑 → product, vehicle_status_A에 새 행 삽입
    """
    zone_map = {
        '서울': 'S',
        '경상도': 'G',
        '경기도': 'K',
        '강원도': 'W'
    }
    zone = zone_map.get(type_)
    if not zone:
        print(f"⚠️ Unknown zone type: {type_}")
        return

    conn, cursor = connect_db()
    try:
        # 1) product 테이블에 삽입
        cursor.execute(
            "INSERT INTO product (product_type, destination_zone_id) VALUES (%s, %s)",
            (data, zone)
        )
        conn.commit()
        product_id = cursor.lastrowid
        print(f"🔄 Inserted product id={product_id}, type={data}, zone={zone}")

        # 2) vehicle_status_A 테이블에 삽입
        cursor.execute(
            "INSERT INTO vehicle_status_A (product_id, destination_zone_id) VALUES (%s, %s)",
            (product_id, zone)
        )
        conn.commit()
        print(f"🔄 Inserted vehicle_status_A for product_id={product_id}, zone={zone}")

    except Error as e:
        print(f"❌ QR insert error: {e}")
    finally:
        cursor.close()
        conn.close()
