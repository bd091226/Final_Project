import paho.mqtt.client as mqtt
from container_DB import update_load_count
from container_config import BROKER, PORT, TOPIC_SUB, TOPIC_PUB

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