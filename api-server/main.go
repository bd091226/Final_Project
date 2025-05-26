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
	ID            uint   `gorm:"primaryKey;autoIncrement"`
	Type          string `gorm:"type:varchar(50)"`
	ZoneId        string `gorm:"type:char(3)"`
	Zone          Zone   ``
	CurrentStatus string `gorm:"type:varchar(50)"`
}

type Zone struct {
	ID          string `gorm:"type:char(3);primaryKey"`
	Name        string `gorm:"type:varchar(50)"`
	X           int    `` // X 좌표
	Y           int    `` // Y 좌표
	LoadMax     int    `` // 최대 적재량
	CurrentLoad int    `` // 현재 적재량
	IsFull      bool   `` // 1: 포화, 0: 여유
}

type Vehicle struct {
	ID          int    `gorm:"primaryKey;autoIncrement"`
	Type        string `gorm:"type:enum('CarA','CarB')"`
	CurrentLoad int    ``
}

type OperationRecord struct {
	ID        int        `gorm:"primaryKey;autoIncrement"`
	VehicleID int        ``
	Vehicle   Vehicle    ``
	Start     *time.Time ``
	End       *time.Time ``
	Status    string     `gorm:"type:enum('InProgress','Completed','Failed')"`
}

type OperationProduct struct {
	ID           int        `gorm:"primaryKey;autoIncrement"`
	ProductID    int        ``
	Product      Product    ``
	ZoneID       int        ``
	Zone         Zone       ``
	LoadOrder    int        ``
	RegisterTime *time.Time ``
	AStartTime   *time.Time ``
	AEndTime     *time.Time ``
	BStartTime   *time.Time ``
	BEndTime     *time.Time ``
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

func initDB() *gorm.DB {
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

func updateHandler[T any](db *gorm.DB) gin.HandlerFunc {
	return func(c *gin.Context) {
		id := c.Param("id") // URL에서 ID를 가져옴
		var item T

		var existingItem T
		result := db.Where("ID = ?", id).First(&existingItem)
		if result.Error != nil {
			if result.Error == gorm.ErrRecordNotFound {
				c.AbortWithStatusJSON(404, gin.H{"error": "리소스를 찾을 수 없습니다."})
			} else {
				c.AbortWithStatusJSON(500, gin.H{"error": fmt.Sprintf("리서스 조회 실패: %v", result.Error.Error())})
			}
			return
		}

		if err := c.ShouldBindJSON(&item); err != nil {
			c.AbortWithStatusJSON(400, gin.H{"error": err.Error()})
			return
		}

		result = db.Model(&existingItem).Updates(item)
		if result.Error != nil {
			c.AbortWithStatusJSON(500, gin.H{"error": fmt.Sprintf("업데이트 실패: %v", result.Error.Error())})
			return
		}
		c.JSON(200, existingItem) // 업데이트된 리소스 반환
	}
}

func deleteHandler[T any](db *gorm.DB) gin.HandlerFunc {
	return func(c *gin.Context) {
		id := c.Param("id") // URL에서 ID를 가져옴
		var item T
		// ID를 사용하여 리소스를 찾고 삭제합니다.
		result := db.Where("ID = ?", id).Delete(&item) // Soft delete (GORM.Model) 또는 Hard delete
		if result.Error != nil {
			c.AbortWithStatusJSON(500, gin.H{"error": fmt.Sprintf("삭제 실패: %v", result.Error.Error())})
			return
		}
		if result.RowsAffected == 0 {
			c.AbortWithStatusJSON(404, gin.H{"error": "리소스를 찾을 수 없거나 이미 삭제되었습니다."})
			return
		}
		c.Status(204) // No Content (성공적으로 삭제되었지만 응답 본문이 없음)
	}
}

func main() {
	db := initDB()
	router := gin.Default()

	api := router.Group("/api")
	{
		// Product
		api.POST("/products", createHandler[Product](db))
		api.GET("/products", getAllHandler[Product](db))
		api.PUT("/products/:id", updateHandler[Product](db))    // ID 기반 업데이트
		api.DELETE("/products/:id", deleteHandler[Product](db)) // ID 기반 삭제

		// Zone
		api.POST("/zones", createHandler[Zone](db))
		api.GET("/zones", getAllHandler[Zone](db))
		api.PUT("/zones/:id", updateHandler[Zone](db))
		api.DELETE("/zones/:id", deleteHandler[Zone](db))

		// Vehicle
		api.POST("/vehicles", createHandler[Vehicle](db))
		api.GET("/vehicles", getAllHandler[Vehicle](db))
		api.PUT("/vehicles/:id", updateHandler[Vehicle](db))
		api.DELETE("/vehicles/:id", deleteHandler[Vehicle](db))

		// OperationRecord
		api.POST("/operation_records", createHandler[OperationRecord](db))
		api.GET("/operation_records", getAllHandler[OperationRecord](db))
		api.PUT("/operation_records/:id", updateHandler[OperationRecord](db))
		api.DELETE("/operation_records/:id", deleteHandler[OperationRecord](db))

		// OperationProduct
		api.POST("/operation_products", createHandler[OperationProduct](db))
		api.GET("/operation_products", getAllHandler[OperationProduct](db))
		api.PUT("/operation_products/:id", updateHandler[OperationProduct](db))
		api.DELETE("/operation_products/:id", deleteHandler[OperationProduct](db))
	}

	port := ":8080"
	log.Printf("서버가 %s 포트에서 실행 중...", port)
	if err := router.Run(port); err != nil {
		log.Fatalf("서버 실행 실패: %v", err)
	}
}
