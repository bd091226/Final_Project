# container_DB_MQTT.py
import mysql.connector
from mysql.connector import Error
import time
import paho.mqtt.client as mqtt
from container_config import DB_CONFIG, BROKER, PORT, TOPIC_SUB, TOPIC_PUB

# --- Database Functions ---
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

# --- MQTT Handler ---
def on_connect(client, userdata, flags, rc):
    if rc == 0:
        print("👉 MQTT connected.")
        client.subscribe(TOPIC_SUB, qos=1)
    else:
        print(f"❌ MQTT connect failed with code {rc}")

def on_message(client, userdata, msg):
    payload = msg.payload.decode()
    try:
        count = int(payload)
        print(f"📥 Received count: {count}")
        conn, cursor = userdata['db']
        update_load_count(cursor, conn, count)
        if count > 5:
            client.publish(TOPIC_PUB, "A차 출발", qos=1)
            print("🔄 Published command: A차 출발")
    except ValueError:
        print("❌ Payload is not an integer.")

def create_mqtt_client(db_conn_tuple):
    client = mqtt.Client(userdata={'db': db_conn_tuple})
    client.on_connect = on_connect
    client.on_message = on_message
    client.connect(BROKER, PORT, keepalive=60)
    return client
