# ssh -i ~/Final_Project/kosta-final-aws-key-250519.pem ubuntu@ec2-13-209-8-231.ap-northeast-2.compute.amazonaws.com
# chmod 400 ~/Final_Project/kosta-final-aws-key-250519.pem
# mysql -u root -p Kosta_Final_250519
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
def button_A(cursor, conn, count, 차량_ID):
    try:
        # 1) 가장 오래된 '등록됨' 택배 조회
        cursor.execute(
            """
            SELECT s.package_id, s.region_id, s.registered_at    -- ID, 구역, 등록 시각
            FROM package s                                       -- 택배 테이블
            WHERE s.package_status = '등록됨'                -- 상태 필터
              AND NOT EXISTS (                                   -- 이미 실린 건 제외
                  SELECT 1 FROM delivery_log us
                  WHERE us.package_id = s.package_id
              )
            ORDER BY s.package_id ASC                             -- 오름차순 정렬
            LIMIT 1                                              -- 한 건
            """
        )
        product = cursor.fetchone()
        if not product:
            print("❌ 등록된 택배 중 실을 수 있는 택배가 없습니다.")
            return -1

        product_id, region_id, registered_at = product

        # 2) count == 1 → 새 운행 생성, else → 기존 비운행중 운행 사용
        if count == 1:
            # 새 운행 기록 생성 (비운행중 상태)
            cursor.execute("""
                INSERT INTO trip_log (vehicle_id, status)         -- trip_log 테이블에 삽입
                VALUES (%s, '비운행중')                           -- 비운행중 상태로 대기
            """, (차량_ID,))
            trip_id = cursor.lastrowid
            print(f"✅ 새 운행 생성 완료: trip_id={trip_id}, vehicle_id={차량_ID}")
        else:
            # count > 1 → 기존 '비운행중' 운행 기록 조회
            cursor.execute("""
                SELECT trip_id                              -- trip_id 조회
                FROM trip_log                               -- trip_log 테이블
                WHERE vehicle_id = %s                       -- 차량 필터
                  AND status     = '비운행중'                -- 비운행중 상태만
                  AND start_time IS NULL                    -- 아직 출발 전인 운행만
                ORDER BY trip_id DESC                       -- 최신 순 정렬
                LIMIT 1                                     -- 한 건만
            """, (차량_ID,))
            row = cursor.fetchone()
            if not row:
                print("❌ 이전 운행_ID 없음 (count > 1인데 기존 운행 찾기 실패)")
                return -1
            trip_id = row[0]
            print(f"🔄 기존 운행_ID 사용: {trip_id}")

        # 3) delivery_log 테이블에 매핑 삽입
        cursor.execute(
            """
            INSERT INTO delivery_log (
                trip_id, package_id, region_id, load_order, registered_at
            ) VALUES (
                %s, %s, %s, %s, %s           -- 운행_ID, 택배_ID, 구역, 순번, 등록 시각
            )
            """,
            (trip_id, product_id, region_id, count, registered_at)
        )
        print(f"✅ 운행_택배 등록 완료: package {product_id} → trip {trip_id}, 순번 {count}")

        # 4) A차 차량 적재 수량 및 LED 상태 업데이트
        if 차량_ID.startswith('A-'):
            cursor.execute(
                """
                UPDATE vehicle                    -- vehicle 테이블
                SET current_load = %s,            -- 현재 적재 수량 설정
                led_status   = CASE 
                WHEN %s = max_load THEN '빨강' 
                    ELSE led_status 
                    END                           -- 만차 시 빨강 표시
                WHERE vehicle_id = %s             -- 해당 차량 필터
                """,
                (count, count, 차량_ID)
            )
            print(f"✅ A차 적재 수량 업데이트 완료: {count}개")

        conn.commit()
        print(trip_id)
        return trip_id

    except Exception as e:
        print(f"❌ 적재 수량 및 운행 등록 실패: {e}")
        return -1


# A차 차량_ID의 현재_적재_수량을 반환하는 함수
# 수정필요!!
def get_A_count(cursor, 차량_ID='A-1000'):
    try:
        cursor.execute("""
            SELECT current_load              -- 차량 적재 수량 조회
            FROM vehicle                    -- vehicle 테이블
            WHERE vehicle_id = %s           -- 해당 차량 필터
        """, (차량_ID,))
        result = cursor.fetchone()
        return result[0] if result else None
    except Exception as e:
        print(f"❌ 적재 수량 조회 실패 (차량 {차량_ID}): {e}")
        return None

# A차 QR에서 출발했다는 신호를 수신 시
# 수정필요!!
def departed_A(conn, cursor, 차량_ID):
    """
    A차가 출발했을 때:
    - '등록됨' 상태의 택배들을 'A차운송중'으로 변경
    - 비운행중 상태이지만 아직 출발하지 않은 운행 기록을 출발 처리
    + 방금 출발 처리된 운행(trip)의 trip_id를 조회해 stdout에 출력 및 반환
    """
    try:
        # 1) 택배 상태 & A차운송 시각 업데이트
        cursor.execute("""
            UPDATE package
            JOIN delivery_log USING (package_id)
            JOIN trip_log      USING (trip_id)
            SET package_status                = 'A차운송중',
                delivery_log.first_transport_time = NOW()
            WHERE trip_log.vehicle_id    = %s
              AND package_status         = '등록됨'
              AND trip_log.start_time    IS NULL
              AND trip_log.status        = '비운행중'
        """, (차량_ID,))

        # 2) 운행 기록 출발 시간 & 상태 변경
        cursor.execute("""
            UPDATE trip_log
            SET start_time = NOW(),
                status     = '운행중'
            WHERE vehicle_id = %s
              AND status     = '비운행중'
              AND start_time IS NULL
        """, (차량_ID,))

        # 3) 차량 LED 상태를 노랑으로 변경
        cursor.execute("""
            UPDATE vehicle
            SET led_status = '노랑'
            WHERE vehicle_id = %s
        """, (차량_ID,))

        conn.commit()

        # 4) 방금 출발 처리된 운행의 trip_id 조회
        cursor.execute("""
            SELECT trip_id
            FROM trip_log
            WHERE vehicle_id = %s
              AND status     = '운행중'
              AND start_time IS NOT NULL
            ORDER BY start_time DESC
            LIMIT 1
        """, (차량_ID,))
        row = cursor.fetchone()
        trip_id = row[0] if row else -1

        # 5) trip_id만 stdout에 찍어서 C 코드에서 읽어오도록 함
        print(trip_id)
        return trip_id

    except Exception as e:
        # 에러 메시지는 stderr로 출력해, stdout 파싱에 방해되지 않도록 함
        import sys
        print(f"❌ departed_A 실패 (차량 {차량_ID}): {e}", file=sys.stderr)
        return -1

# A차 목적지 찾기
def A_destination(운행_ID):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            cur.execute("""
                SELECT region_id                        -- 다음 투입할 구역 ID
                FROM delivery_log                       -- delivery_log 테이블
                WHERE trip_id              = %s         -- 해당 운행 필터
                  AND first_transport_time IS NOT NULL  -- A차 운송이 완료된 건
                  AND input_time           IS NULL      -- 아직 투입되지 않은 건
                ORDER BY load_order ASC                -- 적재 순번 오름차순
                LIMIT 1                                -- 한 건만
            """, (운행_ID,))
            row = cur.fetchone()
            return row[0] if row else None
    finally:
        conn.close()


# A차가 보관함에 도착할 시
# 구역_ID는 나중에 A차의 목적지가 어디인지 알아내서 바꿔야할 것
# 수정필요!!
def zone_arrival_A(conn, cursor, 차량_ID, 구역_ID):
    """
    - 구역 보관 수량 초과 여부 확인
    - 차량 적재량 1 감소
    - 구역 보관 수량 1 증가
    - 운행_택배의 '투입됨', 투입 시각 기록, 택배의 'A차운송중' 변화
    - 보관 수량 확인 후 포화 여부 업데이트
    - 포화 업데이트시 포화 시각 기록
    """
    try:
        # 1) 구역 현재/최대 보관 수량 조회
        cursor.execute("""
            SELECT current_capacity, max_capacity   -- 현재 및 최대 보관 수량
            FROM region                            -- region 테이블
            WHERE region_id = %s                   -- 해당 구역 필터
        """, (구역_ID,))
        current, maximum = cursor.fetchone()
        if current >= maximum:
            print(f"❌ 보관 수량 초과: 현재 {current}, 최대 {maximum}")
            return

        # 2) 차량의 적재 수량 1 감소
        cursor.execute("""
            UPDATE vehicle                         -- vehicle 테이블
            SET current_load = GREATEST(current_load - 1, 0)  -- 적재 수량 감소
            WHERE vehicle_id = %s                  -- 해당 차량 필터
        """, (차량_ID,))

        # 3) 구역의 보관 수량 1 증가
        cursor.execute("""
            UPDATE region                          -- region 테이블
            SET current_capacity = current_capacity + 1         -- 보관 수량 증가
            WHERE region_id = %s                   -- 해당 구역 필터
        """, (구역_ID,))

        # 4) 현재 진행 중인 trip_id 조회
        cursor.execute("""
            SELECT trip_id                          -- 운행 ID 조회
            FROM trip_log                          -- trip_log 테이블
            WHERE vehicle_id = %s AND status = '운행중'  -- 운행 중인 기록
            ORDER BY trip_id DESC                  -- 최신 건
            LIMIT 1                                -- 한 건만
        """, (차량_ID,))
        result = cursor.fetchone()
        if not result:
            print("❌ 운행중인 운행이 없습니다.")
            return
        운행_ID = result[0]

        # 5) 택배 상태를 '투입됨'으로 변경 및 투입 시각 기록
        cursor.execute("""
            UPDATE package                         -- package 테이블
            JOIN delivery_log USING (package_id)   -- delivery_log 조인
            SET package_status = '투입됨',           -- 상태 변경
                delivery_log.input_time = NOW()     -- 투입 시각 기록
            WHERE delivery_log.trip_id  = %s        -- 해당 운행 필터
              AND delivery_log.region_id = %s       -- 해당 구역 필터
              AND delivery_log.input_time IS NULL  -- 아직 투입 전 필터
        """, (운행_ID, 구역_ID))

        # 6) 구역 포화 여부 및 시각 업데이트
        cursor.execute("""
            SELECT current_capacity, max_capacity   -- 재조회용 현재/최대 수량
            FROM region
            WHERE region_id = %s
        """, (구역_ID,))
        current, maximum = cursor.fetchone()
        포화값 = 1 if current == maximum else 0

        cursor.execute("""
            UPDATE region                          -- 포화 여부 설정
            SET is_full = %s                       -- 포화 여부 컬럼
            WHERE region_id = %s                   -- 해당 구역 필터
        """, (포화값, 구역_ID))

        if 포화값 == 1:
            cursor.execute("""
                UPDATE region                      -- 포화 시각 기록
                SET saturated_at = NOW()           -- 현재 시각
                WHERE region_id = %s               -- 해당 구역 필터
            """, (구역_ID,))

        conn.commit()
        print(f"✅ 차량 {차량_ID} → 구역 {구역_ID} 도착 처리 완료 (적재↓, 보관↑, 상태→투입됨)")
        
        print(운행_ID)
        return 운행_ID
    
    except Exception as e:
        print(f"❌ zone_arrival_A 실패 (차량 {차량_ID}): {e}")
        return -1

# A차 운행 완전 종료 (QR 지점으로 도착했을때)
# 수정 필요!!
def end_A(cursor, conn, 차량_ID='A-1000'):
    try:
        # 1) trip_log에서 현재 운행중인 trip_id 조회
        cursor.execute("""
            SELECT trip_id                         -- 운행 ID
            FROM trip_log                         -- 운행 기록 테이블
            WHERE vehicle_id = %s                 -- 해당 차량 필터
              AND status     = '운행중'            -- 운행 중인 상태
            ORDER BY trip_id DESC                  -- 최신 순 정렬
            LIMIT 1                                -- 한 건만
        """, (차량_ID,))
        row = cursor.fetchone()
        if not row:
            print(f"ℹ️ A차({차량_ID}) 운행중인 기록이 없습니다.")
            return
        운행_ID = row[0]

        # 2) 미투입 택배 건수 확인
        cursor.execute("""
            SELECT COUNT(*)                       -- 미투입 택배 수
            FROM delivery_log                     -- 운행-택배 매핑 테이블
            WHERE trip_id        = %s             -- 해당 운행 필터
              AND input_time     IS NULL          -- 아직 투입되지 않은 택배
        """, (운행_ID,))
        남은_건수 = cursor.fetchone()[0]

        if 남은_건수 == 0:
            # 3) trip_log 종료 처리: end_time 기록, status를 '비운행중'으로
            cursor.execute("""
                UPDATE trip_log
                SET end_time = NOW(),               -- 종료 시각 기록
                    status   = '비운행중'           -- 비운행중 상태로 변경
                WHERE trip_id = %s                  -- 해당 운행 필터
            """, (운행_ID,))
            # 4) vehicle LED 상태 초록으로 변경
            cursor.execute("""
                UPDATE vehicle
                SET led_status = '초록'             -- LED를 초록으로
                WHERE vehicle_id = %s               -- 해당 차량 필터
            """, (차량_ID,))
            conn.commit()
            print(f"✅ A차 운행 종료 처리 완료: 운행_ID={운행_ID}")
        else:
            print(f"⏳ A차 운행 중: 남은 미투입 택배 {남은_건수}건")

    except Exception as e:
        conn.rollback()
        print(f"❌ A차 운행 종료 처리 실패: {e}")


# B차 목적지 찾기
# 수정 필요!!
def B_destination(차량_ID='B-1001'):
    conn = get_connection()
    try:
        with conn.cursor() as cur:
            # 1) 포화된 구역 중 가장 빠른 포화 시각의 region_id 조회
            cur.execute("""
                SELECT region_id                    -- 구역 ID
                FROM region                         -- 구역 테이블
                WHERE is_full = 1                   -- 포화된 상태 필터
                  AND saturated_at IS NOT NULL      -- 포화 시각이 기록된 구역만
                ORDER BY saturated_at ASC           -- 가장 오래된 포화 순 정렬
                LIMIT 1                             -- 한 건만 조회
            """)
            row = cur.fetchone()

            if not row:
                print("⚠️ 포화 구역 조회 실패: 등록된 포화 구역이 없습니다.")
                return None
            region_id = row[0]
            print(f"🔍 포화 구역 조회 성공: region_id={region_id}")

            # 2) 운행중 상태로 새 운행 생성
            cur.execute("""
                INSERT INTO trip_log (vehicle_id, status, start_time, destination_region_id)   -- trip_log 테이블에 삽입
                VALUES (%s, '운행중', NOW(), %s)     -- 상태: 운행중, 시작 시각: 현재 시간(NOW)
            """, (차량_ID, region_id))

            trip_id = cur.lastrowid
            conn.commit()
            print(f"✅ 새 운행 생성 성공: trip_id={trip_id}, vehicle_id={차량_ID}, destination_region_id={region_id}")
            return trip_id

    finally:
        conn.close()

# B차 구역함에 도착할 때
# 수정필요!!!!
def zone_arrival_B(conn, cursor, 구역_ID, 차량_ID):
    """
    - region의 현재 보관 수량 조회
    - vehicle current_load 업데이트
    - region current_capacity 및 포화 상태 초기화
    - trip_log에서 운행중인 trip_id 조회
    - package 상태 'B차운송중' 및 second_transport_time 기록
    """
    try:
        # 1) region의 current_capacity 조회
        cursor.execute("""
            SELECT current_capacity                 -- 현재 보관 수량
            FROM region                             -- region 테이블
            WHERE region_id = %s                    -- 해당 구역 필터
        """, (구역_ID,))
        result = cursor.fetchone()
        if result is None:
            print(f"❌ 구역 {구역_ID} 없음")
            return
        저장_수량 = result[0]

        # 2) vehicle current_load 업데이트
        cursor.execute("""
            UPDATE vehicle
            SET current_load = %s                  -- 차량 적재 수량 반영
            WHERE vehicle_id = %s                  -- 해당 차량 필터
        """, (저장_수량, 차량_ID))

        # 3) region current_capacity 및 포화 상태 초기화
        cursor.execute("""
            UPDATE region
            SET current_capacity = 0,              -- 보관 수량 초기화
                is_full          = FALSE,          -- 포화 해제
                saturated_at     = NULL            -- 포화 시각 제거
            WHERE region_id = %s                   -- 해당 구역 필터
        """, (구역_ID,))

        # 4) trip_log에서 운행중인 trip_id 조회
        cursor.execute("""
            SELECT trip_id                          -- 운행 ID 조회
            FROM trip_log                          -- trip_log 테이블
            WHERE vehicle_id = %s                  -- 차량 필터
              AND status     = '운행중'             -- 운행중 상태 필터
            ORDER BY trip_id DESC                   -- 최신 순 정렬
            LIMIT 1                                 -- 한 건만
        """, (차량_ID,))
        row = cursor.fetchone()
        if not row:
            print("❌ 진행중인 운행 없음")
            return
        운행_ID = row[0]

        # 5) package 상태 'B차운송중' 및 second_transport_time 기록
        cursor.execute("""
            UPDATE package
            JOIN delivery_log USING (package_id)      -- delivery_log 조인
            SET package_status          = 'B차운송중', -- 상태 변경
                delivery_log.second_transport_time = NOW() -- B차 운송 시각 기록
            WHERE delivery_log.trip_id    = %s         -- 운행 필터
              AND delivery_log.region_id  = %s         -- 구역 필터
              AND package_status         = '투입됨'     -- 투입된 택배만
              AND delivery_log.second_transport_time IS NULL -- 중복 방지
        """, (운행_ID, 구역_ID))

        conn.commit()
        print(f"✅ B차 도착 처리 완료: 상태→'B차운송중', 구역 {구역_ID} → 차량 {차량_ID}")
    except Exception as e:
        print(f"❌ B차 도착 처리 중 오류: {e}")


# B차 운행 완전 종료 (대기 지점 도착 시)
def end_B(cursor, conn, 차량_ID='B-1001'):
    try:
        # 1) trip_log에서 운행중인 trip_id 조회
        cursor.execute("""
            SELECT trip_id                          -- 운행 ID
            FROM trip_log                          -- trip_log 테이블
            WHERE vehicle_id = %s                  -- 차량 필터
              AND status     = '운행중'             -- 운행중 상태 필터
            ORDER BY trip_id DESC                   -- 최신 순 정렬
            LIMIT 1                                 -- 한 건만
        """, (차량_ID,))
        row = cursor.fetchone()
        if not row:
            print(f"ℹ️ B차({차량_ID}) 운행중인 기록이 없습니다.")
            return
        운행_ID = row[0]

        # 2) 미수거 택배 건수 확인
        cursor.execute("""
            SELECT COUNT(*)                         -- 미수거 택배 수
            FROM delivery_log                      -- delivery_log 테이블
            WHERE trip_id           = %s           -- 운행 필터
              AND second_transport_time IS NULL    -- 아직 회수되지 않은 택배
        """, (운행_ID,))
        남은_건수 = cursor.fetchone()[0]

        if 남은_건수 == 0:
            # 3) trip_log 종료 처리: end_time 기록, status 비운행중
            cursor.execute("""
                UPDATE trip_log
                SET end_time = NOW(),               -- 종료 시각 기록
                    status   = '비운행중'            -- 상태 변경
                WHERE trip_id = %s                  -- 해당 운행 필터
            """, (운행_ID,))
            conn.commit()
            print(f"✅ B차 운행 종료 처리 완료: 운행_ID={운행_ID}")
        else:
            print(f"⏳ B차 운행 중: 남은 미수거 택배 {남은_건수}건")
    except Exception as e:
        conn.rollback()
        print(f"❌ B차 운행 종료 처리 실패: {e}")
        

# A/B차량 현재 좌표 저장 
def update_vehicle_coords(cursor, conn, x, y, vehicle_id):
    try:
        # vehicle 테이블 특정 차량 좌표 업데이트
        cursor.execute("""
            -- vehicle 테이블 특정 차량 coord_x, coord_y 수정
            UPDATE vehicle
               SET coord_x    = %s,  -- 현재 X 좌표
                   coord_y    = %s   -- 현재 Y 좌표
             WHERE vehicle_id = %s   -- 업데이트할 차량의 ID
        """, (x, y, vehicle_id))
        conn.commit()
        print(f"✅ 차량 {vehicle_id} 좌표가 ({x}, {y})로 업데이트되었습니다.")
    except Exception as e:
        conn.rollback()
        print(f"❌ 차량 {vehicle_id} 좌표 업데이트 실패: {e}")
