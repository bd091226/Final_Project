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

# QR을 스캔했을 때 상품 등록시
def qr_insert(type_, product_type):
    zone_map = {
        '서울': '02',
        '경기': '031',
        '경북': '054',
        '강원': '033'
    }
    zone = zone_map.get(type_)
    if not zone:
        print(f"⚠️ Unknown zone type: {type_}")
        return

    conn, cursor = connect_db()
    try:
        cursor.execute(
            """
            INSERT INTO 상품 (상품_종류, 구역_ID, 현재_상태)
            VALUES (%s, %s, '등록됨')
            """,
            (product_type, zone)
        )
        product_id = cursor.lastrowid
        print(f"✅ 상품 등록 완료: ID={product_id}")

        conn.commit()

    except Error as e:
        print(f"❌ QR 등록 중 오류: {e}")
    finally:
        cursor.close()
        conn.close()

# A차에 벨트 버튼을 누를시 A차 적재 수량 1씩 증가
def button_A(cursor, conn, count):
    try:
        # 1. A차의 적재 수량 업데이트
        cursor.execute(
            """
            UPDATE 차량
            SET 현재_적재_수량 = %s
            WHERE 차량_ID = 1
            """,
            (count,)
        )
        print(f"✅ A차 적재 수량 업데이트 완료: {count}개")

        # 2. 가장 최근 등록된 상품 조회
        cursor.execute(
            """
            SELECT 상품_ID, 구역_ID FROM 상품
            ORDER BY 상품_ID DESC
            LIMIT 1
            """
        )
        product = cursor.fetchone()
        if not product:
            print("❌ 등록된 상품이 없습니다.")
            return

        product_id, zone_id = product

        # 3. 운행_기록 생성
        cursor.execute(
            """
            INSERT INTO 운행_기록 (차량_ID, 운행_시작_시각, 운행_상태)
            VALUES (1, NOW(), 0)
            """
        )
        운행_ID = cursor.lastrowid
        print(f"✅ 운행 생성 완료: 운행_ID={운행_ID}")

        # 4. 운행_상품 등록
        cursor.execute(
            """
            INSERT INTO 운행_상품 (
                운행_ID, 상품_ID, 구역_ID, 적재_순번, 등록_시각
            ) VALUES (%s, %s, %s, NULL, NOW())
            """,
            (운행_ID, product_id, zone_id)
        )
        print(f"✅ 운행_상품 등록 완료: 상품 {product_id} → 운행 {운행_ID}")

        conn.commit()

    except Exception as e:
        print(f"❌ 적재 수량 및 운행 등록 실패: {e}")

# A차가 출발했다는 신호를 수신 받을 시
def departed_A(conn, cursor, vehicle_id):
    """
    지정된 차량에 적재된 상품들의 상태를 'A차운송중'으로 변경
    (조건: 현재_상태가 '등록됨'인 상품만)
    """
    try:
        cursor.execute(
            """
            UPDATE 상품
            JOIN 운행_상품 USING (상품_ID)
            JOIN 운행_기록 USING (운행_ID)
            SET 상품.현재_상태 = 'A차운송중'
            WHERE 운행_기록.차량_ID = %s
              AND 상품.현재_상태 = '등록됨'
            """,
            (vehicle_id,)
        )
        conn.commit()
        print(f"✅ 차량 {vehicle_id} 상품 상태 'A차운송중'으로 업데이트 완료")
    except Exception as e:
        print(f"❌ 상태 업데이트 실패 (차량 {vehicle_id}): {e}")

# A차가 구역함 도착시
def zone_arrival_A(conn, cursor, vehicle_id=1, zone_id='02'): 
    """
    차량 도착 처리:
    - 지정된 차량의 적재량 1 감소
    - 지정된 구역의 보관 수량 1 증가
    """
    try:
        # 차량 적재 수량 감소 (최소 0 유지)
        cursor.execute(
            """
            UPDATE 차량
            SET 현재_적재_수량 = GREATEST(현재_적재_수량 - 1, 0)
            WHERE 차량_ID = %s
            """,
            (vehicle_id,)
        )

        # 구역 보관 수량 증가
        cursor.execute(
            """
            UPDATE 구역
            SET 현재_보관_수량 = 현재_보관_수량 + 1
            WHERE 구역_ID = %s
            """,
            (zone_id,)
        )

        conn.commit()
        print(f"✅ 차량 {vehicle_id} → 구역 {zone_id} 도착 처리 완료 (적재↓, 보관↑)")
    except Exception as e:
        print(f"❌ 차량 {vehicle_id} 도착 처리 실패: {e}")
        
# B차 구역함에 도착시 서울의 구역함 보관 수량 0, B차 적재 수량 증가
def transfer_stock_zone_to_vehicle(conn, cursor, zone_id='02', vehicle_id=2):
    try:
        # 1. 구역의 현재 수량 가져오기
        cursor.execute(
            "SELECT 현재_보관_수량 FROM 구역 WHERE 구역_ID = %s",
            (zone_id,)
        )
        result = cursor.fetchone()
        if result is None:
            print(f"❌ 구역 {zone_id} 없음")
            return
        stored_qty = result[0]

        # 2. 차량 적재 수량으로 반영
        cursor.execute(
            "UPDATE 차량 SET 현재_적재_수량 = %s WHERE 차량_ID = %s",
            (stored_qty, vehicle_id)
        )

        # 3. 구역 보관 수량 0으로 초기화
        cursor.execute(
            "UPDATE 구역 SET 현재_보관_수량 = 0 WHERE 구역_ID = %s",
            (zone_id,)
        )

        conn.commit()
        print(f"✅ B차 적재량 ← {zone_id} 구역 보관수량({stored_qty}) 반영 완료, 보관수량 초기화")

    except Exception as e:
        print(f"❌ B차 도착 처리 중 오류: {e}")
