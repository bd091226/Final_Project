# ssh -i ~/Final_Project/kosta-final-aws-key-250519.pem ubuntu@ec2-13-125-155-221.ap-northeast-2.compute.amazonaws.com

#!/usr/bin/env python3
# db_access.py

import pymysql  # pip3 install pymysql

# ——— DB 접속 정보 ———
DB_CONFIG = {
    'host': '13.209.8.231',
    'user': 'user',
    'password': 'Kosta_Final_250519',
    'database': 'Final_Project',
    'port': 3306,
    "charset": "utf8mb4"
}

# DB 연결 객체 반환
def get_connection():
    return pymysql.connect(**DB_CONFIG)

# A차에 벨트 버튼을 누를시 A차 적재 수량 1씩 증가
# 수정필요!!! 임시로 차량 ID를 고정해놓음
_last_운행_ID = None

def button_A(cursor, conn, count, 차량_ID):
    global _last_운행_ID

    try:
        # 1. 등록된 택배 중 가장 오래된 택배 1개 조회
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
            print("❌ 등록된 택배 중 실을 수 있는 택배가 없습니다.")
            print(-1)
            return -1

        product_id, 구역_ID, 등록_시각 = product

        # 2. count == 1 → 운행 생성, else → 기존 운행 사용
        if count == 1:
            cursor.execute("""
                INSERT INTO 운행_기록 (차량_ID, 운행_상태)
                VALUES (%s, '비운행중')
            """, (차량_ID,))
            운행_ID = cursor.lastrowid
            _last_운행_ID = 운행_ID  # 저장해두기
            print(f"✅ 새 운행 생성 완료: 운행_ID={운행_ID}, 차량_ID={차량_ID}")
        else:
            if _last_운행_ID is None:
                print("❌ 이전 운행_ID 없음 (count > 1인데 운행 생성 안 됨)")
                print(-1)
                return -1
            운행_ID = _last_운행_ID
            print(f"🔄 기존 운행_ID 사용: {운행_ID}")

        # 3. 운행_택배 등록
        cursor.execute(
            """
            INSERT INTO 운행_택배 (
                운행_ID, 택배_ID, 구역_ID, 적재_순번, 등록_시각
            ) VALUES (%s, %s, %s, %s, %s)
            """,
            (운행_ID, product_id, 구역_ID, count, 등록_시각)
        )
        print(f"✅ 운행_택배 등록 완료: 택배 {product_id} → 운행 {운행_ID}, 순번 {count}")

        # 4. 구역 보관 수량 초과 확인
        cursor.execute("""
            SELECT 현재_보관_수량, 최대_보관_수량
            FROM 구역 WHERE 구역_ID = %s
        """, (구역_ID,))
        current, maximum = cursor.fetchone()
        if current >= maximum:
            print(f"❌ 보관 수량 초과: 현재 {current}, 최대 {maximum}")
            print(운행_ID)
            return 운행_ID

        # 5. 차량 적재 수량 업데이트
        if 차량_ID.startswith('A-'):
            cursor.execute(
                """
                UPDATE 차량 
                SET 현재_적재_수량 = %s,
                    LED_상태 = CASE WHEN %s = 최대_적재_수량 THEN '빨강' ELSE LED_상태 END
                WHERE 차량_ID = %s
                """,
                (count, count, 차량_ID)
            )
            print(f"✅ A차 적재 수량 업데이트 완료: {count}개")

        # 6. 최대 적재 도달 시 운행 상태 변경
        # 이걸 출발하는 departed도 옮길지 고민필요
        cursor.execute("""
            SELECT 최대_적재_수량 FROM 차량 WHERE 차량_ID = %s
        """, (차량_ID,))
        result = cursor.fetchone()
        if result and count == result[0]:
            cursor.execute("""
                UPDATE 운행_기록
                SET 운행_시작 = NOW(), 운행_상태 = '운행중'
                WHERE 운행_ID = %s
            """, (운행_ID,))
            print("🚚 최대 적재 도달 → 운행_기록 업데이트 완료 (운행중 상태)")

        conn.commit()
        print(운행_ID)
        return 운행_ID

    except Exception as e:
        print(f"❌ 적재 수량 및 운행 등록 실패: {e}")
        print(-1)
        return -1

# A차 차량_ID의 현재_적재_수량을 반환하는 함수
# 수정필요!!
def get_A_count(cursor, 차량_ID='A-1000'):
    try:
        cursor.execute("""
            SELECT 현재_적재_수량
            FROM 차량
            WHERE 차량_ID = %s
        """, (차량_ID,))
        result = cursor.fetchone()
        return result[0] if result else None
    except Exception as e:
        print(f"❌ 적재 수량 조회 실패 (차량 {차량_ID}): {e}")
        return None
    
# A차 A에서 출발했다는 신호를 수신 시
# 수정필요!!
def departed_A(conn, cursor, 차량_ID='A-1000'):
    """
    A차가 A에서 출발했다는 신호를 수신 시:
    - 해당 차량에 적재된 택배들의 상태를 'A차운송중'으로 변경
    - 조건: 현재_상태가 '등록됨'인 택배만
    """
    try:
        cursor.execute("""
            UPDATE 택배
            JOIN 운행_택배 USING (택배_ID)
            JOIN 운행_기록 USING (운행_ID)
            SET 택배.현재_상태 = 'A차운송중'
            
            WHERE 운행_기록.차량_ID = %s
            AND 택배.현재_상태 = '등록됨'
        """, (차량_ID,))
        conn.commit()
        print(f"✅ 차량 {차량_ID} 택배 상태 'A차운송중'으로 업데이트 완료")
    except Exception as e:
        print(f"❌ 상태 업데이트 실패 (차량 {차량_ID}): {e}")
        
# A차 목적지 찾기
def A_destination(운행_ID):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""
                SELECT 구역_ID
                FROM 운행_택배
                WHERE 운행_ID = %s 
                    AND A차운송_시각 IS NOT NULL 
                    AND 투입_시각 IS NULL
                ORDER BY 적재_순번 ASC
                LIMIT 1
            """, (운행_ID,))
            row = cur.fetchone()
            return row[0] if row else None
    finally:
        conn.close()

# A차가 보관함에 도착할 시
# 구역_ID는 나중에 A차의 목적지가 어디인지 알아내서 바꿔야할 것
# 수정필요!!
def zone_arrival_A(conn, cursor, 차량_ID='A-1000', 구역_ID='02'): 
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
        
        
# B차 목적지 찾기
# 포화된(포화_여부=1) 구역 중 가장 빠른 포화시각의 구역ID를 반환
def B_destination():
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""
                SELECT 구역_ID
                FROM 구역
                WHERE 포화_여부 = 1 
                    AND 포화_시각 IS NOT NULL
                ORDER BY 포화_시각 ASC
                LIMIT 1
            """)
            row = cur.fetchone()
            return row[0] if row else None
    finally:
        conn.close()
        
# B차 구역함에 도착시 서울의 구역함 보관 수량 0, B차 적재 수량 증가
# 수정필요!!!! 
def zone_arrival_B(conn, cursor, 구역_ID='02', 차량_ID='B-2000'):
    try:
        # 1. 구역의 현재 수량 가져오기
        cursor.execute("""
            SELECT 현재_보관_수량
            FROM 구역
            WHERE 구역_ID = %s
        """, (구역_ID,))
        result = cursor.fetchone()
        if result is None:
            print(f"❌ 구역 {구역_ID} 없음")
            return
        저장_수량 = result[0]

        # 2. 차량 적재 수량 반영
        cursor.execute("""
            UPDATE 차량
            SET 현재_적재_수량 = %s
            WHERE 차량_ID = %s
        """, (저장_수량, 차량_ID))

        # 3. 구역 보관 수량 0으로 초기화, 포화 여부 0으로 초기화
        cursor.execute("""
            UPDATE 구역
            SET 현재_보관_수량 = 0,
                포화_여부 = 0,
                포화_시각 = NULL
            WHERE 구역_ID = %s
        """, (구역_ID,))
        
        # 4. 현재 운행 중인 운행_ID 가져오기
        cursor.execute("""
            SELECT 운행_ID
            FROM 운행_기록
            WHERE 차량_ID = %s 
                AND 운행_상태 = '운행중'
            ORDER BY 운행_ID DESC
            LIMIT 1
        """, (차량_ID,))
        row = cursor.fetchone()
        if not row:
            print("❌ 진행중인 운행 없음")
            return
        운행_ID = row[0]

        # 5. 택배 상태 업데이트 + B차운송_시각 기록
        cursor.execute("""
            UPDATE 택배
            JOIN 운행_택배 USING (택배_ID)
            SET 택배.현재_상태 = 'B차운송중',
                운행_택배.B차운송_시각 = NOW()
            WHERE 운행_택배.운행_ID = %s
                AND 운행_택배.구역_ID = %s
                AND 택배.현재_상태 = '투입됨'
                AND 운행_택배.B차운송_시각 IS NULL
        """, (운행_ID, 구역_ID))

        conn.commit()
        print(f"✅ B차 도착 처리 완료: 상태→'B차운송중', 구역 {구역_ID} → 차량 {차량_ID}")
        
    except Exception as e:
        print(f"❌ B차 도착 처리 중 오류: {e}")
