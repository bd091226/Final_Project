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
                print("▶️ Connected to MySQL.")
                return conn, conn.cursor()
        except Error as e:
            print(f"❌ MySQL connection error: {e}. Retrying in 5 seconds...")
            time.sleep(5)

def update_load_count(cursor, conn, count):
    """
    Update vehicle_status_A.load_count in the database.
    """
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
    data -> product.product_type
    type_ 매핑 -> product.destination_zone_id
    그리고 product_id를 받아 vehicle_status_A에도 삽입.
    """
    # 지역명 → zone 코드 매핑
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
            """
            INSERT INTO product (product_type, destination_zone_id)
            VALUES (%s, %s)
            """,
            (data, zone)
        )
        conn.commit()
        product_id = cursor.lastrowid
        print(f"✅ product inserted: id={product_id}, type={data}, zone={zone}")

        # 2) vehicle_status_A 테이블에 삽입
        cursor.execute(
            """
            INSERT INTO vehicle_status_A (product_id, destination_zone_id)
            VALUES (%s, %s)
            """,
            (product_id, zone)
        )
        conn.commit()
        print(f"✅ vehicle_status_A inserted: product_id={product_id}, zone={zone}")

    except Error as e:
        print(f"❌ QR insert error: {e}")
    finally:
        cursor.close()
        conn.close()

