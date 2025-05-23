# arrival_sender.py
# ###############################################
# 보관함에 목적지를 도착했다는 신호를 보내는 코드
##############################################
import paho.mqtt.client as mqtt

BROKER = "broker.hivemq.com"
PORT = 1883
TOPIC_ARRIVAL = "myhome/arrival"

def send_arrival():
    client = mqtt.Client()
    client.connect(BROKER, PORT, 60)
    client.loop_start()
    result = client.publish(TOPIC_ARRIVAL, "A차 목적지 도착", qos=1)
    if result[0] == 0:
        print(f"📤 Published '목적지 도착' to {TOPIC_ARRIVAL}")
    else:
        print("❌ Publish failed (arrival)")
    client.loop_stop()
    client.disconnect()
