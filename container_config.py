DB_CONFIG = {
    "user": "Project_19",
    "password": "1234",
    "host": "192.168.137.148",
    "database": "Project_19",
    "charset": "utf8"
}

BROKER        = "broker.hivemq.com"
PORT          = 1883

# MQTT Topics
TOPIC_SUB      = "myhome/button/count"      # 버튼 카운트 수신용
TOPIC_PUB      = "myhome/command"           # A차 출발 명령 발행용
TOPIC_PUB_DIST = "myhome/distance/alert"    # B차 출발(거리 경고) 명령 발행용
TOPIC_STATUS   = "myhome/distance/status"  # B → A 
TOPIC_ARRIVAL  = "myhome/arrival"