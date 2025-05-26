#!/usr/bin/env python3
# db_access.py

import pymysql  # pip3 install pymysql

# ——— DB 접속 정보 ———
DB_CONFIG = {
    "user": "Final",
    "password": "1234",
    "host": "192.168.137.215",
    "database": "delivery",
    "charset": "utf8"
}

def A_start():
    """포화된(포화_여부=1) 구역 중 가장 빠른 포화시각의 구역ID를 반환"""
    conn = pymysql.connect(**DB_CONFIG)
    try:
        with conn.cursor() as cur:
            cur.execute("""
                SELECT 구역_ID
                FROM 운행_상품
                WHERE 운행_ID = %s
                AND A차운송_시각 IS NOT NULL
                AND 투입_시각 IS NULL
                ORDER BY 적재_순번 ASC
                LIMIT 1
            """)
            row = cur.fetchone()
            return row[0] if row else None
    finally:
        conn.close()


def B_start():
    """포화된(포화_여부=1) 구역 중 가장 빠른 포화시각의 구역ID를 반환"""
    conn = pymysql.connect(**DB_CONFIG)
    try:
        with conn.cursor() as cur:
            cur.execute("""
                SELECT 구역_ID
                FROM 구역
                WHERE 포화_여부 = 1 AND 포화_시각 IS NOT NULL
                ORDER BY 포화_시각 ASC
                LIMIT 1
            """)
            row = cur.fetchone()
            return row[0] if row else None
    finally:
        conn.close()


def mark_arrival(zone_id):
    """B 목적지도착 메시지 수신 시, 
        해당 구역의 포화_여부을 0으로 초기화"""
    conn = pymysql.connect(**DB_CONFIG)
    try:
        with conn.cursor() as cur:
            # 포화 여부 0으로 추기화
            cur.execute(
                "UPDATE 구역 SET 포화_여부 = 0, 포화_시각 = NULL WHERE 구역_ID = %s",
                (zone_id,)
            )
            conn.commit()
    finally:
        conn.close()