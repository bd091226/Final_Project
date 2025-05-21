package main

import (
	"fmt"
	"log"
	"os"

	"github.com/gin-gonic/gin"
	"github.com/joho/godotenv"
	"gorm.io/driver/mysql"
	"gorm.io/gorm"

	"github.com/baboyiban/kosta-2/api-server/models"
	"github.com/baboyiban/kosta-2/api-server/mqtt"
)

// 상수 대신 환경 변수 사용
var (
	DB_USER     string
	DB_PASSWORD string
	DB_NAME     string
	DB_HOST     string
	DB_PORT     string
	MQTT_BROKER string
	MQTT_TOPIC  string
)

var db *gorm.DB

func loadEnv() {
	err := godotenv.Load()
	if err != nil {
		log.Println("Error loading .env file, using system env instead")
	}

	// 환경 변수 로드
	DB_USER = getEnv("DB_USER", "root")
	DB_PASSWORD = getEnv("DB_PASSWORD", "password")
	DB_NAME = getEnv("DB_NAME", "mydb")
	DB_HOST = getEnv("DB_HOST", "127.0.0.1")
	DB_PORT = getEnv("DB_PORT", "3306")
	MQTT_BROKER = getEnv("MQTT_BROKER", "tcp://broker.hivemq.com:1883")
	MQTT_TOPIC = getEnv("MQTT_TOPIC", "sensor/data")
}

// 환경 변수가 없을 경우 기본값 제공
func getEnv(key, fallback string) string {
	if value, ok := os.LookupEnv(key); ok {
		return value
	}
	return fallback
}

func setupDB() {
	dsn := fmt.Sprintf("%s:%s@tcp(%s:%s)/%s?charset=utf8mb4&parseTime=True&loc=Local",
		DB_USER, DB_PASSWORD, DB_HOST, DB_PORT, DB_NAME)

	var err error
	db, err = gorm.Open(mysql.Open(dsn), &gorm.Config{})
	if err != nil {
		log.Fatal("Failed to connect to database")
	}

	db.AutoMigrate(&models.SensorData{})
}

func main() {
	loadEnv()
	setupDB()

	// MQTT 구독 시작
	go mqtt.StartMQTTSubscriber(db, MQTT_BROKER, MQTT_TOPIC)

	r := gin.Default()

	// GET API: 저장된 센서 데이터 조회
	r.GET("/data", func(c *gin.Context) {
		var results []models.SensorData
		db.Find(&results)
		c.JSON(200, results)
	})

	log.Println("Server is running on :8080")
	r.Run(":8080")
}
