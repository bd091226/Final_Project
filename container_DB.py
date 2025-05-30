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

# QR을 스캔했을 때 택배 등록시
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
            INSERT INTO 택배 (택배_종류, 구역_ID, 현재_상태)
            VALUES (%s, %s, '등록됨')
            """,
            (product_type, zone)
        )
        product_id = cursor.lastrowid
        print(f"✅ 택배 등록 완료: ID={product_id}")

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
            """
            UPDATE 차량 
            SET 현재_적재_수량 = %s 
            WHERE 차량_ID = 1
            """,
            (count,)
        )
        print(f"✅ A차 적재 수량 업데이트 완료: {count}개")

        # 2. 운행_ID 처리
        if count == 1 or 운행_ID is None:
            cursor.execute(
                """
                INSERT INTO 운행_기록 (차량_ID, 운행_시작_시각, 운행_상태)
                VALUES (1, NOW(), '비운행중')
                """
            )
            운행_ID = cursor.lastrowid
            print(f"✅ 새 운행 생성 완료: 운행_ID={운행_ID}")
        else:
            print(f"🔄 기존 운행_ID 사용: {운행_ID}")

        # 3. 아직 등록되지 않은 가장 오래된 택배 1개 조회
        cursor.execute(
            """
            SELECT s.택배_ID, s.구역_ID, s.등록_시각
            FROM 택배 s
            WHERE s.현재_상태 = '등록됨'
            AND NOT EXISTS (
                SELECT 1 FROM 운행_택배 us WHERE us.택배_ID = s.택배_ID
            )
            ORDER BY s.택배_ID ASC
            LIMIT 1
            """
        )
        product = cursor.fetchone()
        if not product:
            print("❌ 등록 대기 중인 택배이 없습니다.")
            return 운행_ID

        product_id, 구역_ID, 등록_시각 = product

        # 4. 운행_택배 등록
        cursor.execute(
            """
            INSERT INTO 운행_택배 (
                운행_ID, 택배_ID, 구역_ID, 적재_순번, 등록_시각
            ) VALUES (%s, %s, %s, %s, %s)
            """,
            (운행_ID, product_id, 구역_ID, count, 등록_시각)
        )
        print(f"✅ 운행_택배 등록 완료: 택배 {product_id} → 운행 {운행_ID}, 순번 {count}")

        conn.commit()
        return 운행_ID

    except Exception as e:
        print(f"❌ 적재 수량 및 운행 등록 실패: {e}")
        return 운행_ID

# A차가 A출발지에서 출발했다는 신호를 수신 받을 시
def departed_A(conn, cursor, 차량_ID=1):
    """
    지정된 차량에 적재된 택배들의 상태를 'A차운송중'으로 변경
    (조건: 현재_상태가 '등록됨'인 택배만)
    """
    try:
        cursor.execute(
            """
            UPDATE 택배
            JOIN 운행_택배 USING (택배_ID)
            JOIN 운행_기록 USING (운행_ID)
            SET 택배.현재_상태 = 'A차운송중'
            WHERE 운행_기록.차량_ID = %s
              AND 택배.현재_상태 = '등록됨'
            """,
            (차량_ID,)
        )
        conn.commit()
        print(f"✅ 차량 {차량_ID} 택배 상태 'A차운송중'으로 업데이트 완료")
    except Exception as e:
        print(f"❌ 상태 업데이트 실패 (차량 {차량_ID}): {e}")

# A차가 보관함에 도착할 시
# 구역_ID는 나중에 A차의 목적지가 어디인지 알아내서 바꿔야할 것
def zone_arrival_A(conn, cursor, 차량_ID=1, 구역_ID='02'): 
    """
    - 구역 보관 수량 초과 여부 확인
    - 차량 적재량 1 감소
    - 구역 보관 수량 1 증가
    - 운행_택배의 '투입됨', 투입_시각 기록, 택배의 'A차운송중' 변화
    - 보관 수량 확인 후 포화 여부 업데이트
    - 포화 업데이트시 포화_시각 기록
    """
    try:
        # 보관 수량 초과 여부 확인
        cursor.execute(
            """
            SELECT 현재_보관_수량, 최대_보관_수량
            FROM 구역
            WHERE 구역_ID = %s
            """,
            (구역_ID,)
        )
        current, maximum = cursor.fetchone()
        if current >= maximum:
            print(f"❌ 보관 수량 초과: 현재 {current}, 최대 {maximum}")
            return

        # 차량 적재 수량 1 감소
        cursor.execute(
            """
            UPDATE 차량
            SET 현재_적재_수량 = GREATEST(현재_적재_수량 - 1, 0)
            WHERE 차량_ID = %s
            """,
            (차량_ID,)
        )

        # 구역 보관 수량 1 증가
        cursor.execute(
            """
            UPDATE 구역
            SET 현재_보관_수량 = 현재_보관_수량 + 1
            WHERE 구역_ID = %s
            """,
            (구역_ID,)
        )

        # 현재 진행 중인 운행_ID 조회
        cursor.execute(
            """
            SELECT 운행_ID
            FROM 운행_기록
            WHERE 차량_ID = %s AND 운행_상태 = '운행중'
            ORDER BY 운행_ID DESC
            LIMIT 1
            """,
            (차량_ID,)
        )
        result = cursor.fetchone()
        if not result:
            print("❌ 운행중인 운행이 없습니다.")
            return
        운행_ID = result[0]

        # 운행_택배 + 택배 상태 업데이트
        cursor.execute(
            """
            UPDATE 택배
            JOIN 운행_택배 USING (택배_ID)
            SET 택배.현재_상태 = '투입됨',
                운행_택배.투입_시각 = NOW()
            WHERE 운행_택배.운행_ID = %s
              AND 운행_택배.구역_ID = %s
              AND 운행_택배.투입_시각 IS NULL
              AND 택배.현재_상태 = 'A차운송중'
            LIMIT 1
            """,
            (운행_ID, 구역_ID)
        )

        # 보관 수량 확인 후 포화 여부 업데이트
        cursor.execute(
            """
            SELECT 현재_보관_수량, 최대_보관_수량
            FROM 구역
            WHERE 구역_ID = %s
            """,
            (구역_ID,)
        )
        current, maximum = cursor.fetchone()

        포화값 = 1 if current == maximum else 0

        cursor.execute(
            """
            UPDATE 구역
            SET 포화_여부 = %s
            WHERE 구역_ID = %s
            """,
            (포화값, 구역_ID)
        )

        # 포화_여부가 1로 바뀌었을 때만 포화_시각을 기록
        if 포화값 == 1:
            cursor.execute(
                """
                UPDATE 구역
                SET 포화_시각 = NOW()
                WHERE 구역_ID = %s
                """,
                (구역_ID,)
            )
            
        conn.commit()
        print(f"✅ 차량 {차량_ID} → 구역 {구역_ID} 도착 처리 완료 (적재↓, 보관↑, 상태→투입됨)")
    except Exception as e:
        print(f"❌ 차량 {차량_ID} 도착 처리 실패: {e}")
        
# B차 B차출발지에서 출발 
def departed_B(conn, cursor, 차량_ID=2):
    """
    B차 출발 시 운행 상태를 '운행중'으로 갱신
    """
    try:
        cursor.execute(
            """
            UPDATE 운행_기록
            SET 운행_상태 = '운행중'
            WHERE 차량_ID = %s
              AND 운행_상태 = '비운행중'
            ORDER BY 운행_ID DESC
            LIMIT 1
            """,
            (차량_ID,)
        )
        conn.commit()
        print(f"✅ B차 운행 상태 '운행중'으로 변경 완료")
    except Exception as e:
        print(f"❌ B차 운행 상태 변경 실패: {e}")

# B차 구역함에 도착시 서울의 구역함 보관 수량 0, B차 적재 수량 증가
def transfer_stock_zone_to_vehicle(conn, cursor, 구역_ID='02', 차량_ID=2):
    try:
        # 1. 구역의 현재 수량 가져오기
        cursor.execute(
            "SELECT 현재_보관_수량 FROM 구역 WHERE 구역_ID = %s",
            (구역_ID,)
        )
        result = cursor.fetchone()
        if result is None:
            print(f"❌ 구역 {구역_ID} 없음")
            return
        stored_qty = result[0]

        # 2. 차량 적재 수량 반영
        cursor.execute(
            "UPDATE 차량 SET 현재_적재_수량 = %s WHERE 차량_ID = %s",
            (stored_qty, 차량_ID)
        )

        # 3. 구역 보관 수량 0으로 초기화
        cursor.execute(
            "UPDATE 구역 SET 현재_보관_수량 = 0 WHERE 구역_ID = %s",
            (구역_ID,)
        )

        # 4. 포화 여부 0으로 추기화
        cursor.execute(
            "UPDATE 구역 SET 포화_여부 = 0, 포화_시각 = NULL WHERE 구역_ID = %s",
            (구역_ID,)
        )
        
        # 5. 현재 운행 중인 운행_ID 가져오기
        cursor.execute(
            """
            SELECT 운행_ID
            FROM 운행_기록
            WHERE 차량_ID = %s AND 운행_상태 = '진행중'
            ORDER BY 운행_ID DESC
            LIMIT 1
            """,
            (차량_ID,)
        )
        row = cursor.fetchone()
        if not row:
            print("❌ 진행중인 운행 없음")
            return
        운행_ID = row[0]

        # 6. 택배 상태 업데이트 + B차운송_시각 기록
        cursor.execute(
            """
            UPDATE 택배
            JOIN 운행_택배 USING (택배_ID)
            SET 택배.현재_상태 = 'B차운송중',
                운행_택배.B차운송_시각 = NOW()
            WHERE 운행_택배.운행_ID = %s
              AND 운행_택배.구역_ID = %s
              AND 택배.현재_상태 = '투입됨'
              AND 운행_택배.B차운송_시각 IS NULL
            """,
            (운행_ID, 구역_ID)
        )

        conn.commit()
        print(f"✅ B차 도착 처리 완료: 상태→'B차운송중', 구역 {구역_ID} → 차량 {차량_ID}")

    except Exception as e:
        print(f"❌ B차 도착 처리 중 오류: {e}")
