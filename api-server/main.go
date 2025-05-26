package main

import (
	"fmt"
	"log"
	"os"
	"time"

	"github.com/gin-gonic/gin"
	"gorm.io/driver/mysql"
	"gorm.io/gorm"
)

type Product struct {
	gorm.Model
	ID            int    `gorm:"primaryKey"`
	ProductType   string `gorm:"type:varchar(50)"`
	ZoneId        string `gorm:"type:char(3)"`
	Zone          Zone
	CurrentStatus string `gorm:"type:varchar(50)"`
}

type Zone struct {
	ID          string `gorm:"type:char(3);primaryKey"`
	Name        string `gorm:"type:varchar(50)"`
	X           int
	Y           int
	LoadMax     int
	CurrentLoad int
	IsFull      bool // 1: 포화, 0: 여유
}

type Vehicle struct {
	ID          int    `gorm:"primaryKey"`
	Type        string `gorm:"type:enum('CarA','CarB')"`
	CurrentLoad int
}

type OperationRecord struct {
	ID        int `gorm:"primaryKey"`
	VehicleID int
	Vehicle   Vehicle
	Start     *time.Time
	End       *time.Time
	Status    string `gorm:"type:enum('InProgress','Completed','Failed')"`
}

type OperationProduct struct {
	ID           int `gorm:"primaryKey"`
	ProductID    int
	Product      Product
	ZoneID       int
	Zone         Zone
	LoadOrder    int
	RegisterTime *time.Time
	AStartTime   *time.Time
	AEndTime     *time.Time
	BStartTime   *time.Time
	BEndTime     *time.Time
}

func getDSN() string {
	user := lookupEnv("DB_USER", "root")
	pass := lookupEnv("DB_PASSWORD", "password")
	host := lookupEnv("DB_HOST", "127.0.0.1")
	port := lookupEnv("DB_PORT", "3306")
	name := lookupEnv("DB_NAME", "my_database")
	return fmt.Sprintf("%s:%s@tcp(%s:%s)/%s?charset=utf8mb4&parseTime=True&loc=Local",
		user, pass, host, port, name)
}

func lookupEnv(key, fallback string) string {
	if value, exists := os.LookupEnv(key); exists {
		return value
	}
	return fallback
}

func initDatabase() *gorm.DB {
	dsn := getDSN()
	db, err := gorm.Open(mysql.Open(dsn), &gorm.Config{})
	if err != nil {
		log.Fatalf("데이터베이스 연결 실패: %v", err)
	}

	err = db.AutoMigrate(
		&Product{},
		&Zone{},
		&Vehicle{},
		&OperationRecord{},
		&OperationProduct{},
	)
	if err != nil {
		log.Fatalf("데이터베이스 마이그레이션 실패: %v", err)
	}
	log.Println("데이터베이스 연결 및 마이그레이션 성공")
	return db
}

func createHandler[T any](db *gorm.DB) gin.HandlerFunc {
	return func(c *gin.Context) {
		var item T
		if err := c.ShouldBindJSON(&item); err != nil {
			c.AbortWithStatusJSON(400, gin.H{"error": err.Error()})
			return
		}

		result := db.Create(&item)
		if result.Error != nil {
			c.AbortWithStatusJSON(500, gin.H{"error": fmt.Sprintf("생성 실패: %v", result.Error.Error())})
			return
		}
		c.JSON(201, item)
	}
}

func getAllHandler[T any](db *gorm.DB) gin.HandlerFunc {
	return func(c *gin.Context) {
		var items []T
		result := db.Find(&items)
		if result.Error != nil {
			c.AbortWithStatusJSON(500, gin.H{"error": fmt.Sprintf("조회 실패: %v", result.Error.Error())})
			return
		}
		c.JSON(200, items)
	}
}

func main() {
	db := initDatabase()

	router := gin.Default()

	api := router.Group("/api")
	{
		api.POST("/products", createHandler[Product](db))
		api.GET("/products", getAllHandler[Product](db))

		api.POST("/zones", createHandler[Zone](db))
		api.GET("/zones", getAllHandler[Zone](db))

		api.POST("/vehicles", createHandler[Vehicle](db))
		api.GET("/vehicles", getAllHandler[Vehicle](db))

		api.POST("/operation_records", createHandler[OperationRecord](db))
		api.GET("/operation_records", getAllHandler[OperationRecord](db))

		api.POST("/operation_products", createHandler[OperationProduct](db))
		api.GET("/operation_products", getAllHandler[OperationProduct](db))
	}

	port := ":8080"
	log.Printf("서버가 %s 포트에서 실행 중...", port)
	if err := router.Run(port); err != nil {
		log.Fatalf("서버 실행 실패: %v", err)
	}
}
