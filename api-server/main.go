package main

import (
	"fmt"
	"os"

	"github.com/gin-gonic/gin"
	"gorm.io/driver/mysql"
	"gorm.io/gorm"
)

type Data struct {
	ID    uint   `json:"id" gorm:"primaryKey"`
	Name  string `json:"name"`
	Value string `json:"value"`
}

func main() {
	dsn := getDSN()
	db, err := gorm.Open(mysql.Open(dsn), &gorm.Config{})
	if err != nil {
		panic("failed to connect database")
	}

	db.AutoMigrate(&Data{})

	r := gin.Default()

	r.POST("/data", func(c *gin.Context) {
		var data Data
		if err := c.ShouldBindJSON(&data); err != nil {
			c.AbortWithStatusJSON(400, gin.H{"error": err.Error()})
			return
		}
		db.Create(&data)
		c.JSON(201, data)
	})

	r.GET("/data", func(c *gin.Context) {
		var datas []Data
		db.Find(&datas)
		c.JSON(200, datas)
	})

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
