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
