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

def update_load_order(cursor, conn, count):
    """
    MQTT로부터 수신된 count 값을 A차(차량_ID=1)의 현재_적재_수량에 반영.
    """
    try:
        cursor.execute(
            """
            UPDATE 차량
            SET 현재_적재_수량 = %s
            WHERE 차량_ID = 1
            """,
            (count,)
        )
        conn.commit()
        print(f"✅ A차 적재 수량 업데이트 완료: {count}개")
    except Exception as e:
        print(f"❌ 적재 수량 업데이트 실패: {e}")

def handle_qr_insert(type_, product_type):
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
        # 1. 상품 등록
        cursor.execute(
            """
            INSERT INTO 상품 (상품_종류, 구역_ID, 현재_상태)
            VALUES (%s, %s, '등록됨')
            """,
            (product_type, zone)
        )
        product_id = cursor.lastrowid
        print(f"✅ 상품 등록 완료: ID={product_id}")

        # 2. 운행_기록 생성 (자동 생성 / 하나의 상품에 하나의 운행 기준)
        cursor.execute(
            """
            INSERT INTO 운행_기록 (차량_ID, 운행_시작_시각, 운행_상태)
            VALUES (1, NOW(), 0)
            """
        )
        운행_ID = cursor.lastrowid
        print(f"✅ 운행 생성 완료: 운행_ID={운행_ID}")

        # 3. 운행_상품에 등록 (등록_시각 = NOW())
        cursor.execute(
            """
            INSERT INTO 운행_상품 (
                운행_ID, 상품_ID, 구역_ID, 적재_순번, 등록_시각
            )
            SELECT %s, 상품_ID, 구역_ID, NULL, NOW()
            FROM 상품
            WHERE 상품_ID = %s
            """,
            (운행_ID, product_id)
        )
        print(f"✅ 운행_상품 등록 완료: 상품 {product_id} → 운행 {운행_ID}")

        conn.commit()

    except Error as e:
        print(f"❌ QR 등록 중 오류: {e}")
    finally:
        cursor.close()
        conn.close()
