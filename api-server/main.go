package main

import (
	"fmt"
	"os"
	"time"

	"github.com/gin-gonic/gin"
	"gorm.io/driver/mysql"
	"gorm.io/gorm"
)

// 테스트 환경
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

// 배포 환경
// type 상품 struct {
// 	상품_ID int    `json:"상품_ID" gorm:"primaryKey;autoIncrement"`
// 	상품_종류 string `json:"상품_종류" gorm:"type:char(50)"`
// 	구역_ID string `json:"구역_ID" gorm:"type:char(3);primaryKey;foreignKey:구역_ID"`
// 	현재_상태 string `json:"현재_상태" gorm:"type:char(50)"`
// }

// type 구역 struct {
// 	구역_ID    string `json:"구역_ID" gorm:"type:char(3);primaryKey"`
// 	구역_명     string `json:"구역_명" gorm:"type:char(50)"`
// 	좌표_X     int    `json:"좌표_X"`
// 	좌표_Y     int    `json:"좌표_Y"`
// 	최대_보관_수량 int    `json:"최대_보관_수량"`
// 	현재_보관_수량 int    `json:"현재_보관_수량"`
// 	포화_여부    bool   `json:"포화_여부"`
// }

// type 차량 struct {
// 	차량_ID    int    `json:"차량_ID" gorm:"primaryKey;autoIncrement"`
// 	차량_종류    string `json:"차량_종류" gorm:"enum('A차','B차')"`
// 	현재_적재_수량 int    `json:"현재_적재_수량"`
// }

// type 운행_기록 struct {
// 	운행_ID    int        `json:"운행_ID" gorm:"primaryKey;autoIncrement"`
// 	차량_ID    int        `json:"foreignKey:차량_ID"`
// 	운행_시작_시간 *time.Time `json:"운행_시작_시간"`
// 	운행_종료_시간 *time.Time `json:"운행_종료_시간"`
// 	운행_상태    string     `json:"운행_상태" gorm:"enum('진행중','완료','실패')"`
// }

// type 운행_상품 struct {
// 	운행_ID   int        `json:"운행_ID" gorm:"primaryKey;foreignKey:운행_ID"`
// 	상품_ID   int        `json:"상품_ID" gorm:"primaryKey;foreignKey:상품_ID"`
// 	구역_ID   string     `json:"구역_ID" gorm:"type:char(3);primaryKey"`
// 	적재_순번   int        `json:"적재_순번"`
// 	등록_시각   *time.Time `json:"등록_시각"`
// 	A차운송_시각 *time.Time `json:"A차운송_시각"`
// 	투입_시각   *time.Time `json:"투입_시각"`
// 	B차운송_시각 *time.Time `json:"B차운송_시각"`
// 	완료_시각   *time.Time `json:"완료_시각"`
// }

func main() {
	dsn := getDSN()
	db, err := gorm.Open(mysql.Open(dsn), &gorm.Config{})
	if err != nil {
		panic("failed to connect database")
	}

	// err = db.AutoMigrate(&상품{}, &구역{}, &차량{}, &운행_기록{}, &운행_상품{})
	err = db.AutoMigrate(&Product{})
	if err != nil {
		fmt.Printf("Database migration failed: %v\n", err)
		panic("failed to migrate database schema")
	} else {
		fmt.Println("Database migration successful!")
	}

	r := gin.Default()
	// product
	r.POST("/data/product", func(c *gin.Context) {
		var product Product
		if err := c.ShouldBindJSON(&product); err != nil {
			c.AbortWithStatusJSON(400, gin.H{"error": err.Error()})
			return
		}
		result := db.Create(&product)
		if result.Error != nil {
			c.AbortWithStatusJSON(500, gin.H{"error": result.Error.Error()})
			return
		}
		c.JSON(201, product)
	})
	r.GET("/data/product", func(c *gin.Context) {
		var products []Product
		result := db.Find(&products)
		if result.Error != nil {
			c.AbortWithStatusJSON(500, gin.H{"error": result.Error.Error()})
			return
		}
		c.JSON(200, products)
	})
	// zone

	fmt.Println("Server listening on :8080")
	r.Run(":8080")
}

func getDSN() string {
	user := lookupEnv("DB_USER", "")
	pass := lookupEnv("DB_PASSWORD", "")
	host := lookupEnv("DB_HOST", "")
	port := lookupEnv("DB_PORT", "")
	name := lookupEnv("DB_NAME", "")

	return fmt.Sprintf("%s:%s@tcp(%s:%s)/%s?charset=utf8mb4&parseTime=True&loc=Local",
		user, pass, host, port, name)
}

func lookupEnv(key, fallback string) string {
	if value, exists := os.LookupEnv(key); exists {
		return value
	}
	return fallback
}
