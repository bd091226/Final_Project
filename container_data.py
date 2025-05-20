import sqlite3
import datetime
import paho.mqtt.client as mqtt

# ─── MQTT & DB 설정 ───────────────────────────────────────────
BROKER   = "broker.hivemq.com"
PORT     = 1883
TOPIC_IN = "myhome/piA/qr"
DB_FILE  = "qr_data.db"

# ─── SQLite 데이터베이스 초기화 ───────────────────────────────
conn = sqlite3.connect(DB_FILE, check_same_thread=False)
cursor = conn.cursor()
cursor.execute('''
CREATE TABLE IF NOT EXISTS qr_codes (
    id        INTEGER PRIMARY KEY AUTOINCREMENT,
    data      TEXT NOT NULL,
    received  DATETIME NOT NULL
)
''')
conn.commit()

# ─── MQTT 콜백 ────────────────────────────────────────────────
def on_connect(client, userdata, flags, rc):
    print(f"[PiB] Connected with result code {rc}")
    client.subscribe(TOPIC_IN)
    print(f"[PiB] Subscribed to topic: {TOPIC_IN}")

def on_message(client, userdata, msg):
    qr_data = msg.payload.decode()
    now = datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')
    cursor.execute("INSERT INTO qr_codes (data, received) VALUES (?, ?)", (qr_data, now))
    conn.commit()
    print(f"[PiB] Saved QR: '{qr_data}' at {now}")

# ─── MQTT 클라이언트 시작 ────────────────────────────────────
client = mqtt.Client("PiB_QR_Receiver")
client.on_connect = on_connect
client.on_message = on_message
client.connect(BROKER, PORT, 60)
client.loop_forever()

