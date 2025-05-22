# container_config.py
# Configuration for DB and MQTT

DB_CONFIG = {
    "user": "Project_19",
    "password": "1234",
    "host": "192.168.137.148",
    "database": "Project_19",
    "charset": "utf8"
}

BROKER    = "broker.hivemq.com"
PORT      = 1883
TOPIC_SUB = "myhome/button/count"
TOPIC_PUB = "myhome/command"
