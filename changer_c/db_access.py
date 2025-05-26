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


def fetch_saturated_zone():
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