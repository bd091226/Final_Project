DB_CONFIG = {
    "user": "user",
    "password": "kosta-final-250519",
    "host": "13.125.155.221",
    "database": "Final_Project",
    "charset": "utf8mb4",
}

BROKER        = "broker.hivemq.com"
PORT          = 1883

# MQTT Topics
TOPIC_COUNT      = "myhome/button/count"      # 버튼 카운트 수신용
TOPIC_PUB      = "myhome/command"           # A차 출발 명령 발행용
TOPIC_PUB_DIST = "myhome/distance/alert"    # B차 출발(거리 경고) 명령 발행용
TOPIC_STATUS   = "myhome/distance/status"  # B → A 
TOPIC_ARRIVAL  = "myhome/arrival"