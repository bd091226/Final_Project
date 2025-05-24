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
def button_A(cursor, conn, count, 운행_ID=None):
    try:
        # 1. A차의 적재 수량 업데이트
        cursor.execute(
            "UPDATE 차량 SET 현재_적재_수량 = %s WHERE 차량_ID = 1",
            (count,)
        )
        print(f"✅ A차 적재 수량 업데이트 완료: {count}개")

        # 2. 최근 상품 조회
        cursor.execute(
            """
<<<<<<< HEAD
            SELECT 상품_ID, 구역_ID, 등록_시각 
            FROM 상품 
            ORDER BY 상품_ID DESC LIMIT 1"""
=======
            SELECT 상품_ID, 구역_ID FROM 상품
            ORDER BY 상품_ID DESC
            LIMIT 1
            """
>>>>>>> c6fadb3c0b52a4097acf62e65c77a901c2532eee
        )
        product = cursor.fetchone()
        if not product:
            print("❌ 등록된 상품이 없습니다.")
<<<<<<< HEAD
            return None
        product_id, zone_id, 등록_시각 = product
=======
            return

        product_id, zone_id = product
>>>>>>> c6fadb3c0b52a4097acf62e65c77a901c2532eee

        # 3. 운행_기록 생성 or 주어진 ID 사용
        if count == 1:
            cursor.execute(
<<<<<<< HEAD
                "INSERT INTO 운행_기록 (차량_ID, 운행_시작_시각, 운행_상태) VALUES (1, NOW(), '비운행중')"
=======
                """
                INSERT INTO 운행_기록 (차량_ID, 운행_시작_시각, 운행_상태)
                VALUES (1, NOW(), '진행중')
                """
>>>>>>> c6fadb3c0b52a4097acf62e65c77a901c2532eee
            )
            운행_ID = cursor.lastrowid
            print(f"✅ 새 운행 생성 완료: 운행_ID={운행_ID}")
        elif 운행_ID is None:
            print("❌ 운행_ID가 전달되지 않았습니다. count > 1인 경우 운행_ID 필요.")
            return None
        else:
<<<<<<< HEAD
            print(f"🔄 기존 운행_ID 사용: {운행_ID}")
=======
            cursor.execute(
                """
                SELECT 운행_ID
                FROM 운행_기록
                WHERE 차량_ID = 1 AND 운행_상태 = '진행중'
                ORDER BY 운행_ID DESC
                LIMIT 1
                """
            )
            result = cursor.fetchone()
            if not result:
                print("❌ 진행중인 운행을 찾을 수 없습니다.")
                return
            운행_ID = result[0]
            print(f"🔄 기존 운행에 등록: 운행_ID={운행_ID}")
>>>>>>> c6fadb3c0b52a4097acf62e65c77a901c2532eee

        # 4. 운행_상품 등록
        cursor.execute(
            """
            INSERT INTO 운행_상품 (
                운행_ID, 상품_ID, 구역_ID, 적재_순번, 등록_시각
            ) VALUES (%s, %s, %s, %s, NOW())
            """,
            (운행_ID, product_id, zone_id, count)
        )
        print(f"✅ 운행_상품 등록 완료: 상품 {product_id} → 운행 {운행_ID}, 순번 {count}")

        conn.commit()
        return 운행_ID  # 새로 만든 ID 또는 재사용한 ID 반환

    except Exception as e:
        print(f"❌ 적재 수량 및 운행 등록 실패: {e}")
        return None


# A차가 A출발지에서 출발했다는 신호를 수신 받을 시
def departed_A(conn, cursor, vehicle_id=1):
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

def zone_arrival_A(conn, cursor, vehicle_id=1, zone_id='02'): 
    """
    차량 도착 처리:
    - 적재량 1 감소
    - 구역 보관 수량 1 증가
    - 해당 구역에 속한 상품 상태를 '투입됨'으로 변경
    - 운행_상품.투입_시각도 기록
    """
    try:
        # 차량 적재 수량 감소
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

        # 현재 진행 중인 운행_ID 조회
        cursor.execute(
            """
            SELECT 운행_ID
            FROM 운행_기록
            WHERE 차량_ID = %s AND 운행_상태 = '진행중'
            ORDER BY 운행_ID DESC
            LIMIT 1
            """,
            (vehicle_id,)
        )
        result = cursor.fetchone()
        if not result:
            print("❌ 진행중 운행이 없습니다.")
            return
        운행_ID = result[0]

        # 운행_상품 + 상품 상태 업데이트
        cursor.execute(
            """
            UPDATE 상품
            JOIN 운행_상품 USING (상품_ID)
            SET 상품.현재_상태 = '투입됨',
                운행_상품.투입_시각 = NOW()
            WHERE 운행_상품.운행_ID = %s
              AND 운행_상품.구역_ID = %s
              AND 운행_상품.투입_시각 IS NULL
              AND 상품.현재_상태 = 'A차운송중'
            LIMIT 1
            """,
            (운행_ID, zone_id)
        )

        conn.commit()
        print(f"✅ 차량 {vehicle_id} → 구역 {zone_id} 도착 처리 완료 (적재↓, 보관↑, 상태→투입됨)")
    except Exception as e:
        print(f"❌ 차량 {vehicle_id} 도착 처리 실패: {e}")

# # A차 다음 목적지 탐색
# def get_next_zone_for_unloading(cursor, 운행_ID):
#     """
#     운행_ID 기준으로 아직 투입되지 않은 상품 중,
#     A차운송_시각은 존재하고, 투입_시각은 NULL인 상품의 구역_ID 중 가장 빠른 순번 하나 반환
#     """
#     try:
#         cursor.execute(
#             """
#             SELECT 구역_ID
#             FROM 운행_상품
#             WHERE 운행_ID = %s
#               AND A차운송_시각 IS NOT NULL
#               AND 투입_시각 IS NULL
#             ORDER BY 적재_순번 ASC
#             LIMIT 1
#             """,
#             (운행_ID,)
#         )
#         result = cursor.fetchone()
#         if result:
#             return result[0]
#         return None
#     except Exception as e:
#         print(f"❌ 하차 구역 조회 실패: {e}")
#         return None

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

        # 2. 차량 적재 수량 반영
        cursor.execute(
            "UPDATE 차량 SET 현재_적재_수량 = %s WHERE 차량_ID = %s",
            (stored_qty, vehicle_id)
        )

        # 3. 구역 보관 수량 0으로 초기화
        cursor.execute(
            "UPDATE 구역 SET 현재_보관_수량 = 0 WHERE 구역_ID = %s",
            (zone_id,)
        )

        # 4. 현재 운행 중인 운행_ID 가져오기
        cursor.execute(
            """
            SELECT 운행_ID
            FROM 운행_기록
            WHERE 차량_ID = %s AND 운행_상태 = '진행중'
            ORDER BY 운행_ID DESC
            LIMIT 1
            """,
            (vehicle_id,)
        )
        row = cursor.fetchone()
        if not row:
            print("❌ 진행중인 운행 없음")
            return
        운행_ID = row[0]

        # 5. 상품 상태 업데이트 + B차운송_시각 기록
        cursor.execute(
            """
            UPDATE 상품
            JOIN 운행_상품 USING (상품_ID)
            SET 상품.현재_상태 = 'B차운송중',
                운행_상품.B차운송_시각 = NOW()
            WHERE 운행_상품.운행_ID = %s
              AND 운행_상품.구역_ID = %s
              AND 상품.현재_상태 = '투입됨'
              AND 운행_상품.B차운송_시각 IS NULL
            """,
            (운행_ID, zone_id)
        )

        conn.commit()
        print(f"✅ B차 도착 처리 완료: 상태→'B차운송중', 구역 {zone_id} → 차량 {vehicle_id}")

    except Exception as e:
        print(f"❌ B차 도착 처리 중 오류: {e}")
